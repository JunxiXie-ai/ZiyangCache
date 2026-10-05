#pragma once

namespace ZiyangCache
{

template <typename Key, typename Value>
class KICachePolicy
{
public:
    virtual ~KICachePolicy() = default;

    // Insert or update a key-value pair
    virtual void put(Key key, Value value) = 0;

    // Return true if key exists, and store the value in `value`
    virtual bool get(Key key, Value& value) = 0;

    // Return the value directly
    virtual Value get(Key key) = 0;
};

} // namespace ZiyangCache