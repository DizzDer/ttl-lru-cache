# TTL LRU Cache

[![CI](https://github.com/DizzDer/ttl-lru-cache/actions/workflows/ci.yml/badge.svg)](https://github.com/DizzDer/ttl-lru-cache/actions/workflows/ci.yml)

A header-only C++17 cache combining a fixed entry budget, per-entry TTL, LRU eviction and synchronized access. Useful for bounded in-process memoization where callers need explicit expiration and observable hit/miss behavior.

```cpp
#include "ttl_lru_cache.hpp"
using namespace std::chrono_literals;
caching::ttl_lru_cache<std::string, std::string> cache(128);
cache.put("profile:42", "Ada", 5min);
auto value = cache.get("profile:42"); // optional<string>; a copy
```

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requires CMake 3.20+ and C++17. Run `./build/demo` (Visual Studio: `build/Release/demo.exe`). Expected output:

```text
Ada
profile:7 = evicted
hits=1 misses=1 evictions=1
```

## Semantics

- Capacity counts entries, not bytes. Zero capacity is rejected.
- TTL starts at insertion/update. Reads update LRU order but do not extend TTL.
- A key expires when `now >= expires_at`, including the exact boundary.
- Nonpositive TTL removes a key; expiration-time overflow throws.
- `put` purges expired entries before evicting a live LRU entry.
- `get` returns a copy so eviction on another thread cannot invalidate a returned reference.
- `stats().stored` includes stale entries until visited or explicitly purged.
- There is no background maintenance thread. Use `purge_expired()` when idle reclamation matters.

## Complexity and constraints

`get` and `erase` are expected O(1), with hash-table worst cases. `put` and `purge_expired` are O(capacity) because expiration cleanup scans the list. This is an explicit tradeoff for a small implementation with bounded memory; it is unsuitable for enormous write-heavy caches without further indexing.

Key must be copyable and hashable with `std::hash<Key>`. Hash and equality must not throw. Value must be move-constructible and copy-constructible; destructors must not throw. User-defined hash, equality, copy and clock functions must not re-enter the cache, because they can run while its mutex is held.

## Validation

An injected clock makes TTL tests deterministic without sleeping. Tests cover LRU order, replacement, exact expiry, invalid TTL, stale-before-live eviction, purge, duration overflow and 16000 concurrent put/get pairs. CI covers three operating systems in Debug/Release and Linux ASan/UBSan.

See [architecture](docs/architecture.md), [validation](docs/validation.md) and [contributing](CONTRIBUTING.md). MIT licensed. Intended as a compact reference component, not a distributed cache or a claim of production usage.

