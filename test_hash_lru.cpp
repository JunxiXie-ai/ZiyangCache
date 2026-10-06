#include <iostream>
#include <string>

#include "KLruCache.h"

int main()
{
    ZiyangCache::KHashLruCaches<int, std::string> cache(8, 4);

    cache.put(1, "A");
    cache.put(2, "B");
    cache.put(3, "C");
    cache.put(4, "D");

    std::string value;

    if (cache.get(1, value))
    {
        std::cout << "key 1: " << value << std::endl;
    }

    if (cache.get(3, value))
    {
        std::cout << "key 3: " << value << std::endl;
    }

    if (!cache.get(100, value))
    {
        std::cout << "key 100 not found" << std::endl;
    }

    return 0;
}