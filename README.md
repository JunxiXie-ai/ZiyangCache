# ZiyangCache

ZiyangCache is a C++17 cache library implementing multiple cache replacement policies, concurrency optimizations, and adaptive caching strategies.

The project includes implementations of LRU, LRU-K, LFU, frequency aging, cache sharding, and an ARC-style adaptive cache, together with workload-based benchmarks for comparing cache hit rates.

## Features

- LRU cache with hash-map lookup and doubly linked list eviction
- LRU-K for filtering low-frequency / one-time accesses
- HashLRU cache sharding to reduce lock contention
- LFU cache with frequency-based eviction
- Frequency aging to reduce the impact of stale historical hot keys
- HashLFU cache sharding
- ARC-style adaptive cache combining recency and frequency
- Thread-safe cache operations using mutexes
- Unified cache interface through C++ templates and polymorphism
- Benchmark suite for different workload patterns
- CMake-based build system

## Cache Policies

### LRU

The LRU cache combines:

- `std::unordered_map` for average O(1) key lookup
- A doubly linked list for O(1) recency updates and eviction

Recently accessed nodes are moved toward the most-recently-used end of the list, while the least recently used node is evicted when the cache reaches capacity.

### LRU-K

LRU-K adds an access-history layer before entries enter the main cache.

An entry must reach a configurable access threshold `K` before being promoted into the main LRU cache.

This helps filter one-time or low-frequency accesses that could otherwise pollute the cache.

### HashLRU

HashLRU divides the cache into multiple independent LRU shards.

```text
Hash(key) % shard_count
        |
        v
   target shard
```

Each shard maintains its own cache and mutex, allowing accesses to different shards to proceed independently and reducing lock contention under concurrent workloads.

### LFU

LFU evicts entries based on access frequency.

The implementation maintains:

```
key -> node
frequency -> list of nodes
```

When multiple entries have the same frequency, the older entry within that frequency group is evicted first.

### LFU Aging

Pure LFU can allow historically popular entries to remain in the cache long after they stop being useful.

Frequency aging periodically reduces accumulated frequency values when the average frequency becomes too large.

This prevents stale hot entries from permanently dominating the cache.

### HashLFU

HashLFU applies the same sharding strategy used by HashLRU to LFU caches.

Each shard independently maintains:

- LFU eviction state
- frequency information
- mutex protection
- frequency aging

This improves concurrency at the cost of making eviction decisions local to each shard instead of globally optimal.

## ARC

The ARC implementation combines an LRU-oriented cache region with an LFU-oriented cache region.

```
                 KArcCache
                 /       \
          ArcLruPart     ArcLfuPart
          /      \       /       \
       main    ghost   main     ghost
```

New entries first enter the LRU-oriented region.

Repeated accesses increase an entry's access count. Once the configured transformation threshold is reached, the entry can also be promoted into the LFU-oriented region.

Both regions maintain ghost caches that remember recently evicted entries.

A ghost hit is used to dynamically shift capacity between the LRU and LFU regions:

```
LRU ghost hit
    -> decrease LFU capacity
    -> increase LRU capacity

LFU ghost hit
    -> decrease LRU capacity
    -> increase LFU capacity
```

This allows the cache to adapt between recency-oriented and frequency-oriented workloads.

## Thread Safety

The cache implementations use `std::mutex` and `std::lock_guard` to protect shared cache state.

Hash-based sharding further reduces contention by distributing keys across independent cache instances.

## Project Structure

```
ZiyangCache/
├── KArcCache/
│   ├── KArcCache.h
│   ├── KArcCacheNode.h
│   ├── KArcLfuPart.h
│   └── KArcLruPart.h
├── KICachePolicy.h
├── KLruCache.h
├── KLfuCache.h
├── testAllCachePolicy.cpp
├── CMakeLists.txt
├── hitTest.jpg
└── README.md
```

## Benchmark

The benchmark compares:

- LRU
- LFU
- ARC
- LRU-K
- LFU with frequency aging

under three different workload patterns.

### 1. Hot Data Access

A small set of keys receives most accesses, while a much larger cold-key set is accessed occasionally.

| Policy    | Hit Rate   |
| --------- | ---------- |
| LRU       | 49.42%     |
| LFU       | **67.03%** |
| ARC       | 65.97%     |
| LRU-K     | 54.84%     |
| LFU-Aging | 66.87%     |

LFU performs best because access frequency is a strong predictor of future accesses in a stable hot-key workload.

### 2. Loop Scan

The workload repeatedly scans a key range much larger than the cache capacity.

| Policy    | Hit Rate  |
| --------- | --------- |
| LRU       | 4.63%     |
| LFU       | 8.71%     |
| ARC       | **9.67%** |
| LRU-K     | 4.86%     |
| LFU-Aging | 8.77%     |

Sequential scans can heavily pollute an LRU cache because newly scanned entries continuously replace older entries.

ARC performs best in this workload by combining recency and frequency information and adapting the balance between them.

### 3. Workload Shift

The access pattern changes across several phases, including:

- concentrated hot-key access
- large-range random access
- sequential scans
- local random access
- mixed workloads

| Policy    | Hit Rate   |
| --------- | ---------- |
| LRU       | 55.08%     |
| LFU       | 41.20%     |
| ARC       | **59.25%** |
| LRU-K     | 54.51%     |
| LFU-Aging | 38.88%     |

ARC performs best because it can dynamically adjust between recency-oriented and frequency-oriented caching as the workload changes.

> Benchmark workloads use randomized access generation, so exact results may vary between runs.

 

## Build

### Requirements

- C++17 compatible compiler
- CMake 3.16+

### Build with CMake

```
mkdir -p build
cd build
cmake ..
cmake --build .
```

Run the benchmark:

```
./testAllCachePolicy
```

### Build directly with g++

```
g++ -std=c++17 -O2 testAllCachePolicy.cpp -o testAllCachePolicy
./testAllCachePolicy
```

## Key Design Trade-offs

### LRU vs LFU

LRU adapts quickly to recent workload changes but can be vulnerable to scan pollution.

LFU preserves frequently accessed entries well but can retain historical hot entries for too long.

### Cache Sharding

Sharding improves concurrency by reducing lock contention.

However, each shard makes eviction decisions independently, so the globally optimal victim may not always be selected.

### ARC

ARC introduces additional implementation complexity and metadata overhead, but it can adapt more effectively when workload characteristics change over time.

## What I Learned

This project helped me practice:

- C++ templates and polymorphism
- smart pointers and ownership management
- doubly linked list design
- hash-based indexing
- thread synchronization with mutexes
- cache replacement algorithms
- cache sharding
- adaptive cache policies
- workload-based benchmarking
- CMake project organization

## Acknowledgements

This project is a learning-oriented implementation based on
[KamaCache](https://github.com/youngyangyang04/KamaCache).

The original project is licensed under the GNU General Public License v3.0.
This project contains modifications and extensions for learning and experimentation.

## License

This project is licensed under the GNU General Public License v3.0.
See [LICENSE](LICENSE) for details.