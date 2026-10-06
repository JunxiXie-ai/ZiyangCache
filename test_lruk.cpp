#include <iostream>
#include <string>

#include "KLruCache.h"

int main()
{
    ZiyangCache::KLruKCache<int, std::string> cache(2, 10, 2);

    // Promote A to main cache
    cache.put(1, "A");
    cache.put(1, "A");

    // Promote B to main cache
    cache.put(2, "B");
    cache.put(2, "B");

    // C is accessed only once, so it should stay in history
    cache.put(3, "C");

    std::cout << "key 1: " << cache.get(1) << std::endl;
    std::cout << "key 2: " << cache.get(2) << std::endl;

    return 0;
}