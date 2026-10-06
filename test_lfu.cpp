#include <iostream>
#include <string>

#include "KLfuCache.h"

int main()
{
    ZiyangCache::KLfuCache<int, std::string> cache(2, 2);

    cache.put(1, "A");
    cache.put(2, "B");

    std::string value;

    // Make key 1 very hot
    cache.get(1, value);
    cache.get(1, value);
    cache.get(1, value);
    cache.get(1, value);

    // Access key 2 too, which should eventually trigger frequency aging
    cache.get(2, value);
    cache.get(2, value);

    cache.put(3, "C");

    std::cout << "key 1 exists: "
              << cache.get(1, value) << std::endl;

    std::cout << "key 2 exists: "
              << cache.get(2, value) << std::endl;

    std::cout << "key 3 exists: "
              << cache.get(3, value) << std::endl;

    return 0;
}