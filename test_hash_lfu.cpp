#include <iostream>
#include <string>

#include "KLfuCache.h"

int main()
{
    ZiyangCache::KHashLfuCache<int, std::string> cache(8, 4);

    cache.put(1, "A");
    cache.put(2, "B");
    cache.put(3, "C");
    cache.put(4, "D");

    std::string value;

    std::cout << "key 1 exists: "
              << cache.get(1, value) << std::endl;

    std::cout << "key 3 exists: "
              << cache.get(3, value) << std::endl;

    std::cout << "key 100 exists: "
              << cache.get(100, value) << std::endl;

    cache.purge();

    std::cout << "key 1 after purge: "
              << cache.get(1, value) << std::endl;

    return 0;
}