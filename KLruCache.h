#pragma once 

#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "KICachePolicy.h"

namespace ZiyangCache
{

// forward declaration
template<typename Key, typename Value> class KLruCache;

template<typename Key, typename Value>
class LruNode 
{
private:
    Key key_;
    Value value_;
    size_t accessCount_;  // access count
    std::weak_ptr<LruNode<Key, Value>> prev_;  // break the cycle reference
    std::shared_ptr<LruNode<Key, Value>> next_;

public:
    LruNode(Key key, Value value)
        : key_(key)
        , value_(value)
        , accessCount_(1) 
    {}

    // provide necessary accessors
    Key getKey() const { return key_; }
    Value getValue() const { return value_; }
    void setValue(const Value& value) { value_ = value; }
    size_t getAccessCount() const { return accessCount_; }
    void incrementAccessCount() { ++accessCount_; }

    friend class KLruCache<Key, Value>;
};

template<typename Key, typename Value>
class KLruCache : public KICachePolicy<Key, Value>
{
public:
    using LruNodeType = LruNode<Key, Value>;
    using NodePtr = std::shared_ptr<LruNodeType>;
    using NodeMap = std::unordered_map<Key, NodePtr>;

    KLruCache(int capacity)
        : capacity_(capacity)
    {
        initializeList();
    }

    ~KLruCache() override = default;

    // add a new node to the cache
    void put(Key key, Value value) override
    {
        if (capacity_ <= 0)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodeMap_.find(key);

        if (it != nodeMap_.end()) // node already exists
        {
            updateExistingNode(it->second, value); // update the value of the existing node
            return;
        }

        addNewNode(key, value);
    }

    bool get(Key key, Value& value) override
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodeMap_.find(key);

        if (it != nodeMap_.end())
        {
            moveToMostRecent(it->second);
            value = it->second->getValue();
            return true;
        }

        return false;
    }

    Value get(Key key) override 
    {
        Value value{};
        get(key, value);
        return value;
    }

    // 删除指定元素
    void remove(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodeMap_.find(key);

        if (it != nodeMap_.end())
        {
            removeNode(it->second);
            nodeMap_.erase(it);
        }
    }

private:
    void initializeList()
    {
        // 创建首尾虚拟节点
        dummyHead_ = std::make_shared<LruNodeType>(Key(), Value());
        dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());

        dummyHead_->next_ = dummyTail_;
        dummyTail_->prev_ = dummyHead_;
    }

    void updateExistingNode(NodePtr node, const Value& value) 
    {
        node->setValue(value);
        moveToMostRecent(node);
    }

    void addNewNode(const Key& key, const Value& value) 
    {
        if (nodeMap_.size() >= capacity_) 
        {
            evictLeastRecent(); // evict the least recent node
        }

        NodePtr newNode = std::make_shared<LruNodeType>(key, value);
        insertNode(newNode);
        nodeMap_[key] = newNode;
    }

    void removeNode(NodePtr node) 
    {
        if(!node->prev_.expired() && node->next_) 
        {
            auto prev = node->prev_.lock();
            prev->next_ = node->next_;
            node->next_->prev_ = prev;
            node->next_ = nullptr;
        }
    }

    // insert the node at the tail
    void insertNode(NodePtr node) 
    {
        node->next_ = dummyTail_;
        node->prev_ = dummyTail_->prev_;
        dummyTail_->prev_.lock()->next_ = node;
        dummyTail_->prev_ = node;
    }

    // move the node to the most recent position
    void moveToMostRecent(NodePtr node) 
    {
        removeNode(node);
        insertNode(node);
    }

    // evict the least recent node
    void evictLeastRecent() 
    {
        NodePtr leastRecent = dummyHead_->next_;
        removeNode(leastRecent);
        nodeMap_.erase(leastRecent->getKey());
    }

private:
    int           capacity_;
    NodeMap       nodeMap_;
    std::mutex    mutex_;
    NodePtr       dummyHead_;
    NodePtr       dummyTail_;

    
};
} // namespace ZiyangCache