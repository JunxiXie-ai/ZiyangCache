#pragma once

#include "../KICachePolicy.h"
#include "KArcLfuPart.h"
#include "KArcLruPart.h"

#include <memory>

namespace ZiyangCache
{

template<typename Key, typename Value>
class KArcCache : public KICachePolicy<Key, Value>
{
public:
    explicit KArcCache(
        size_t capacity = 10,
        size_t transformThreshold = 2
    )
        : capacity_(capacity)
        , transformThreshold_(transformThreshold)
        , lruPart_(
              std::make_unique<ArcLruPart<Key, Value>>(
                  capacity,
                  transformThreshold
              )
          )
        , lfuPart_(
              std::make_unique<ArcLfuPart<Key, Value>>(
                  capacity,
                  transformThreshold
              )
          )
    {}

    ~KArcCache() override = default;

    void put(Key key, Value value) override
    {
        checkGhostCaches(key);

        // Check whether the key already exists in LFU
        bool inLfu = lfuPart_->contain(key);

        // Update the LRU part
        lruPart_->put(key, value);

        // If the key already exists in LFU, update it there too
        if (inLfu)
        {
            lfuPart_->put(key, value);
        }
    }

    bool get(Key key, Value& value) override
    {
        checkGhostCaches(key);

        bool shouldTransform = false;

        if (lruPart_->get(key, value, shouldTransform))
        {
            if (shouldTransform)
            {
                lfuPart_->put(key, value);
            }

            return true;
        }

        return lfuPart_->get(key, value);
    }

    Value get(Key key) override
    {
        Value value{};
        get(key, value);
        return value;
    }

private:
    bool checkGhostCaches(Key key)
    {
        bool inGhost = false;

        // Hit in LRU ghost cache:
        // give more capacity to LRU
        if (lruPart_->checkGhost(key))
        {
            if (lfuPart_->decreaseCapacity())
            {
                lruPart_->increaseCapacity();
            }

            inGhost = true;
        }
        // Hit in LFU ghost cache:
        // give more capacity to LFU
        else if (lfuPart_->checkGhost(key))
        {
            if (lruPart_->decreaseCapacity())
            {
                lfuPart_->increaseCapacity();
            }

            inGhost = true;
        }

        return inGhost;
    }

private:
    size_t capacity_;
    size_t transformThreshold_;

    std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
    std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;
};

} // namespace ZiyangCache