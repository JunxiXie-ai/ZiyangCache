#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "KICachePolicy.h"
#include "KLfuCache.h"
#include "KLruCache.h"
#include "KArcCache/KArcCache.h"

class Timer
{
public:
    Timer()
        : start_(std::chrono::high_resolution_clock::now())
    {}

    double elapsed()
    {
        auto now = std::chrono::high_resolution_clock::now();

        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_
        ).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

void printResults(
    const std::string& testName,
    int capacity,
    const std::vector<int>& getOperations,
    const std::vector<int>& hits)
{
    std::cout << "=== " << testName << " ===" << std::endl;
    std::cout << "Cache capacity: " << capacity << std::endl;

    std::vector<std::string> names;

    if (hits.size() == 3)
    {
        names = {"LRU", "LFU", "ARC"};
    }
    else if (hits.size() == 4)
    {
        names = {"LRU", "LFU", "ARC", "LRU-K"};
    }
    else if (hits.size() == 5)
    {
        names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    }

    for (size_t i = 0; i < hits.size(); ++i)
    {
        double hitRate =
            100.0 * hits[i] / getOperations[i];

        std::cout
            << names[i]
            << " - Hit Rate: "
            << std::fixed
            << std::setprecision(2)
            << hitRate
            << "% ("
            << hits[i]
            << "/"
            << getOperations[i]
            << ")"
            << std::endl;
    }

    std::cout << std::endl;
}

void testHotDataAccess()
{
    std::cout << "\n=== Scenario 1: Hot Data Access ==="
              << std::endl;

    const int CAPACITY = 20;
    const int OPERATIONS = 500000;
    const int HOT_KEYS = 20;
    const int COLD_KEYS = 5000;

    ZiyangCache::KLruCache<int, std::string> lru(CAPACITY);
    ZiyangCache::KLfuCache<int, std::string> lfu(CAPACITY);
    ZiyangCache::KArcCache<int, std::string> arc(CAPACITY);

    ZiyangCache::KLruKCache<int, std::string> lruk(
        CAPACITY,
        HOT_KEYS + COLD_KEYS,
        2
    );

    // Same LFU implementation, but with a lower aging threshold
    ZiyangCache::KLfuCache<int, std::string> lfuAging(
        CAPACITY,
        20000
    );

    std::random_device rd;
    std::mt19937 gen(rd());

    std::array<
        ZiyangCache::KICachePolicy<int, std::string>*,
        5
    > caches = {
        &lru,
        &lfu,
        &arc,
        &lruk,
        &lfuAging
    };

    std::vector<int> hits(5, 0);
    std::vector<int> getOperations(5, 0);

    for (int i = 0; i < caches.size(); ++i)
    {
        // Warm up with hot keys
        for (int key = 0; key < HOT_KEYS; ++key)
        {
            caches[i]->put(
                key,
                "value" + std::to_string(key)
            );
        }

        for (int op = 0; op < OPERATIONS; ++op)
        {
            // 30% write, 70% read
            bool isPut = (gen() % 100 < 30);

            int key;

            // 70% hot data, 30% cold data
            if (gen() % 100 < 70)
            {
                key = gen() % HOT_KEYS;
            }
            else
            {
                key =
                    HOT_KEYS +
                    (gen() % COLD_KEYS);
            }

            if (isPut)
            {
                std::string value =
                    "value" +
                    std::to_string(key) +
                    "_v" +
                    std::to_string(op % 100);

                caches[i]->put(key, value);
            }
            else
            {
                std::string result;

                getOperations[i]++;

                if (caches[i]->get(key, result))
                {
                    hits[i]++;
                }
            }
        }
    }

    printResults(
        "Hot Data Access",
        CAPACITY,
        getOperations,
        hits
    );
}

void testLoopPattern()
{
    std::cout << "\n=== Scenario 2: Loop Scan ==="
              << std::endl;

    const int CAPACITY = 50;
    const int LOOP_SIZE = 500;
    const int OPERATIONS = 200000;

    ZiyangCache::KLruCache<int, std::string> lru(CAPACITY);
    ZiyangCache::KLfuCache<int, std::string> lfu(CAPACITY);
    ZiyangCache::KArcCache<int, std::string> arc(CAPACITY);

    ZiyangCache::KLruKCache<int, std::string> lruk(
        CAPACITY,
        LOOP_SIZE * 2,
        2
    );

    ZiyangCache::KLfuCache<int, std::string> lfuAging(
        CAPACITY,
        3000
    );

    std::array<
        ZiyangCache::KICachePolicy<int, std::string>*,
        5
    > caches = {
        &lru,
        &lfu,
        &arc,
        &lruk,
        &lfuAging
    };

    std::vector<int> hits(5, 0);
    std::vector<int> getOperations(5, 0);

    std::random_device rd;
    std::mt19937 gen(rd());

    for (int i = 0; i < caches.size(); ++i)
    {
        // Warm up 20% of the loop range
        for (int key = 0; key < LOOP_SIZE / 5; ++key)
        {
            caches[i]->put(
                key,
                "loop" + std::to_string(key)
            );
        }

        int currentPos = 0;

        for (int op = 0; op < OPERATIONS; ++op)
        {
            bool isPut = (gen() % 100 < 20);

            int key;

            if (op % 100 < 60)
            {
                // 60% sequential scan
                key = currentPos;
                currentPos =
                    (currentPos + 1) % LOOP_SIZE;
            }
            else if (op % 100 < 90)
            {
                // 30% random access
                key = gen() % LOOP_SIZE;
            }
            else
            {
                // 10% outside the normal range
                key =
                    LOOP_SIZE +
                    (gen() % LOOP_SIZE);
            }

            if (isPut)
            {
                std::string value =
                    "loop" +
                    std::to_string(key) +
                    "_v" +
                    std::to_string(op % 100);

                caches[i]->put(key, value);
            }
            else
            {
                std::string result;

                getOperations[i]++;

                if (caches[i]->get(key, result))
                {
                    hits[i]++;
                }
            }
        }
    }

    printResults(
        "Loop Scan",
        CAPACITY,
        getOperations,
        hits
    );
}

void testWorkloadShift()
{
    std::cout
        << "\n=== Scenario 3: Workload Shift ==="
        << std::endl;

    const int CAPACITY = 30;
    const int OPERATIONS = 80000;
    const int PHASE_LENGTH = OPERATIONS / 5;

    ZiyangCache::KLruCache<int, std::string> lru(CAPACITY);
    ZiyangCache::KLfuCache<int, std::string> lfu(CAPACITY);
    ZiyangCache::KArcCache<int, std::string> arc(CAPACITY);

    ZiyangCache::KLruKCache<int, std::string> lruk(
        CAPACITY,
        500,
        2
    );

    ZiyangCache::KLfuCache<int, std::string> lfuAging(
        CAPACITY,
        10000
    );

    std::random_device rd;
    std::mt19937 gen(rd());

    std::array<
        ZiyangCache::KICachePolicy<int, std::string>*,
        5
    > caches = {
        &lru,
        &lfu,
        &arc,
        &lruk,
        &lfuAging
    };

    std::vector<int> hits(5, 0);
    std::vector<int> getOperations(5, 0);

    for (int i = 0; i < caches.size(); ++i)
    {
        for (int key = 0; key < 30; ++key)
        {
            caches[i]->put(
                key,
                "init" + std::to_string(key)
            );
        }

        for (int op = 0; op < OPERATIONS; ++op)
        {
            int phase = op / PHASE_LENGTH;

            int putProbability;

            switch (phase)
            {
                case 0:
                    putProbability = 15;
                    break;

                case 1:
                    putProbability = 30;
                    break;

                case 2:
                    putProbability = 10;
                    break;

                case 3:
                    putProbability = 25;
                    break;

                case 4:
                    putProbability = 20;
                    break;

                default:
                    putProbability = 20;
            }

            bool isPut =
                (gen() % 100 < putProbability);

            int key;

            if (op < PHASE_LENGTH)
            {
                // Phase 1: concentrated hot data
                key = gen() % 5;
            }
            else if (op < PHASE_LENGTH * 2)
            {
                // Phase 2: large random range
                key = gen() % 400;
            }
            else if (op < PHASE_LENGTH * 3)
            {
                // Phase 3: sequential scan
                key =
                    (op - PHASE_LENGTH * 2)
                    % 100;
            }
            else if (op < PHASE_LENGTH * 4)
            {
                // Phase 4: local random access
                int locality =
                    (op / 800) % 5;

                key =
                    locality * 15 +
                    (gen() % 15);
            }
            else
            {
                // Phase 5: mixed workload
                int r = gen() % 100;

                if (r < 40)
                {
                    key = gen() % 5;
                }
                else if (r < 70)
                {
                    key =
                        5 +
                        (gen() % 45);
                }
                else
                {
                    key =
                        50 +
                        (gen() % 350);
                }
            }

            if (isPut)
            {
                std::string value =
                    "value" +
                    std::to_string(key) +
                    "_p" +
                    std::to_string(phase);

                caches[i]->put(key, value);
            }
            else
            {
                std::string result;

                getOperations[i]++;

                if (caches[i]->get(key, result))
                {
                    hits[i]++;
                }
            }
        }
    }

    printResults(
        "Workload Shift",
        CAPACITY,
        getOperations,
        hits
    );
}

int main()
{
    testHotDataAccess();
    testLoopPattern();
    testWorkloadShift();

    return 0;
}