#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "KICachePolicy.h"

namespace ZiyangCache
{

template<typename Key, typename Value> class KLfuCache;

template<typename Key, typename Value>
class FreqList
{
private:
    struct Node
    {
        int freq; // frequency of the key
        Key key;
        Value value;

        std::weak_ptr<Node> pre; // previous node changed to weak_ptr to break the circular reference
        std::shared_ptr<Node> next;

        Node()
            : freq(1), next(nullptr)
        {}

        Node(Key key, Value value)
            : freq(1), key(key), value(value), next(nullptr) // initialize the frequency to 1, key, value, and next to nullptr
        {}
    };

    using NodePtr = std::shared_ptr<Node>;

    int freq_;
    NodePtr head_;
    NodePtr tail_;

public:
    explicit FreqList(int n)
        : freq_(n)
    {
        head_ = std::make_shared<Node>();
        tail_ = std::make_shared<Node>();

        head_->next = tail_;
        tail_->pre = head_;
    }

    bool isEmpty() const
    {
        return head_->next == tail_;
    }

    void addNode(NodePtr node)
    {
        if (!node || !head_ || !tail_)
            return;

        node->pre = tail_->pre;
        node->next = tail_;

        tail_->pre.lock()->next = node;
        tail_->pre = node;
    }

    void removeNode(NodePtr node)
    {
        if (!node || !head_ || !tail_)
            return;

        if (node->pre.expired() || !node->next)
            return;

        auto pre = node->pre.lock();

        pre->next = node->next;
        node->next->pre = pre;

        node->next = nullptr;
    }

    NodePtr getFirstNode() const
    {
        return head_->next;
    }

    friend class KLfuCache<Key, Value>;
};

// ------------------------------------------------------------------------------------------------
template <typename Key, typename Value>
class KLfuCache : public KICachePolicy<Key, Value>
{
public:
    using Node = typename FreqList<Key, Value>::Node;
    using NodePtr = std::shared_ptr<Node>;
    using NodeMap = std::unordered_map<Key, NodePtr>; // map of the keys to the nodes

    KLfuCache(int capacity, int maxAverageNum = 1000000)
        : capacity_(capacity),
          minFreq_(INT8_MAX),
          maxAverageNum_(maxAverageNum),
          curAverageNum_(0),
          curTotalNum_(0)
    {}

    ~KLfuCache() override = default;

    void put(Key key, Value value) override
    {
        if (capacity_ == 0)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodeMap_.find(key);

        if (it != nodeMap_.end())
        {
            it->second->value = value;
            getInternal(it->second, value);
            return;
        }

        putInternal(key, value);
    }

    bool get(Key key, Value& value) override
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodeMap_.find(key);

        if (it != nodeMap_.end())
        {
            getInternal(it->second, value);
            return true;
        }

        return false;
    }

    Value get(Key key) override
    {
        Value value;
        get(key, value);
        return value;
    }

    void purge()
    {
        nodeMap_.clear();
        freqToFreqList_.clear();
    }

private:
    void putInternal(Key key, Value value); // put the key-value pair into the cache
    void getInternal(NodePtr node, Value& value); // get the value of the key

    void kickOut(); // kick out the least frequently used key

    void removeFromFreqList(NodePtr node); // remove the node from the frequency list
    void addToFreqList(NodePtr node); // add the node to the frequency list

    void addFreqNum(); // add the frequency number
    void decreaseFreqNum(int num); // decrease the frequency number
    void handleOverMaxAverageNum(); // handle the case where the average number of keys is over the maximum average number
    void updateMinFreq(); // update the minimum frequency

private:
    int capacity_; // capacity of the cache
    int minFreq_; // minimum frequency

    int maxAverageNum_; // maximum average number of keys
    int curAverageNum_; // current average number of keys
    int curTotalNum_; // current total number of keys

    std::mutex mutex_; // mutex to protect the cache

    NodeMap nodeMap_; // map of the keys to the nodes

    std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_; // map of the frequencies to the frequency lists
};

template<typename Key, typename Value>
void KLfuCache<Key, Value>::getInternal(NodePtr node, Value& value)
{
    // Return the current value
    value = node->value;

    // Remove the node from its current frequency list
    removeFromFreqList(node);

    // Increase frequency
    node->freq++;

    // Add it to the new frequency list
    addToFreqList(node);

    // If the old minimum-frequency list becomes empty,
    // update minFreq_
    if (node->freq - 1 == minFreq_ &&
        freqToFreqList_[node->freq - 1]->isEmpty())
    {
        minFreq_++;
    }

    // Update total / average frequency statistics
    addFreqNum();
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::putInternal(Key key, Value value)
{
    // If the cache is full, evict one node first
    if (nodeMap_.size() == capacity_)
    {
        kickOut();
    }

    // Create a new node
    NodePtr node = std::make_shared<Node>(key, value);

    // Add it to the key -> node map
    nodeMap_[key] = node;

    // A new node starts with frequency = 1
    addToFreqList(node);

    // Update frequency statistics
    addFreqNum();

    // New nodes always start at freq = 1
    minFreq_ = std::min(minFreq_, 1);
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::kickOut()
{
    NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();

    removeFromFreqList(node);

    nodeMap_.erase(node->key);

    decreaseFreqNum(node->freq);
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::removeFromFreqList(NodePtr node)
{
    if (!node)
        return;

    auto freq = node->freq;

    freqToFreqList_[freq]->removeNode(node);
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::addToFreqList(NodePtr node)
{
    if (!node)
        return;

    auto freq = node->freq;

    if (freqToFreqList_.find(freq) == freqToFreqList_.end())
    {
        freqToFreqList_[freq] = new FreqList<Key, Value>(freq);
    }

    freqToFreqList_[freq]->addNode(node);
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::addFreqNum()
{
    curTotalNum_++;

    if (nodeMap_.empty())
        curAverageNum_ = 0;
    else
        curAverageNum_ = curTotalNum_ / nodeMap_.size();

    if (curAverageNum_ > maxAverageNum_)
    {
        handleOverMaxAverageNum();
    }
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::decreaseFreqNum(int num)
{
    curTotalNum_ -= num;

    if (nodeMap_.empty())
        curAverageNum_ = 0;
    else
        curAverageNum_ = curTotalNum_ / nodeMap_.size();
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::handleOverMaxAverageNum()
{
    if (nodeMap_.empty())
        return;

    // Decay the frequency of every cached node
    for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it)
    {
        if (!it->second)
            continue;

        NodePtr node = it->second;

        // Remove from the old frequency list
        removeFromFreqList(node);

        int oldFreq = node->freq;

        int decay = maxAverageNum_ / 2;

        node->freq -= decay; // decrease the frequency of the node

        if (node->freq < 1)
            node->freq = 1; // set the frequency of the node to 1 if it is less than 1

        // Update total frequency according to the change
        int delta = node->freq - oldFreq; // calculate the change in the total number of keys
        curTotalNum_ += delta; // update the total number of keys

        // Put the node into its new frequency list
        addToFreqList(node);
    }

    curAverageNum_ = curTotalNum_ / nodeMap_.size();

    updateMinFreq();
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::updateMinFreq()
{
    minFreq_ = INT8_MAX;

    for (const auto& pair : nodeMap_)
    {
        if (pair.second)
        {
            minFreq_ = std::min(minFreq_, pair.second->freq);
        }
    }

    if (nodeMap_.empty())
        minFreq_ = 0;
}

// ------------------------------------------------------------------------------------------------
template<typename Key, typename Value>
class KHashLfuCache
{
public:
    KHashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10)
        : capacity_(capacity)
        , sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
    {
        size_t sliceSize = std::ceil(
            capacity / static_cast<double>(sliceNum_)
        );

        for (int i = 0; i < sliceNum_; ++i)
        {
            lfuSliceCaches_.emplace_back(
                new KLfuCache<Key, Value>(sliceSize, maxAverageNum)
            );
        }
    }

    void put(Key key, Value value)
    {
        size_t sliceIndex = Hash(key) % sliceNum_;
        lfuSliceCaches_[sliceIndex]->put(key, value);
    }

    bool get(Key key, Value& value)
    {
        size_t sliceIndex = Hash(key) % sliceNum_;
        return lfuSliceCaches_[sliceIndex]->get(key, value);
    }

    Value get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    void purge() // purge the cache
    {
        for (auto& lfuSliceCache : lfuSliceCaches_)
        {
            lfuSliceCache->purge();
        }
    }

private:
    size_t Hash(Key key)
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }

private:
    size_t capacity_;
    int sliceNum_;

    std::vector<std::unique_ptr<KLfuCache<Key, Value>>> lfuSliceCaches_;
};
} // namespace ZiyangCache