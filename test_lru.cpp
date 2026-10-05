#include <iostream>
#include <string>

#include "KLruCache.h"

int main()
{
    ZiyangCache::KLruCache<int, std::string> cache(3);

    cache.put(1, "A");
    cache.put(2, "B");
    cache.put(3, "C");

    std::string value;

    if (cache.get(1, value))
    {
        std::cout << "key 1: " << value << std::endl;
    }

    // 访问过 key 1 后，它变成最近使用
    // 此时最久没使用的是 key 2
    cache.put(4, "D");

    std::cout << "key 2 exists: "
              << cache.get(2, value) << std::endl;

    std::cout << "key 1 exists: "
              << cache.get(1, value) << std::endl;

    std::cout << "key 3 exists: "
              << cache.get(3, value) << std::endl;

    std::cout << "key 4 exists: "
              << cache.get(4, value) << std::endl;

    return 0;
}