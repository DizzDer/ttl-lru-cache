# Architecture and tradeoffs

## Data structures

A doubly linked list stores entries in most-recently-used order. An unordered map maps keys to list iterators. A successful get splices an entry to the front without copying its key/value. Expiration removes both records. Capacity eviction removes the list tail.

All public operations take one mutex. Returned values are copies. Iterators and references to internal storage never escape. This is a simple linearizable API, rather than a lock-free or sharded implementation.

## Time

The default clock is `std::chrono::steady_clock`; wall-clock corrections cannot extend or shorten TTL. Tests inject a manually advanced clock. A custom clock must not move backwards and its `now()` must be safe for the way the cache is used.

Expiration uses a checked time-point addition. Nonpositive TTL is defined as invalidation. TTL is fixed at write time; sliding expiration is not supported.

## Exception behavior

Replacement constructs the new list node before removing the old one, so a failing value construction does not destroy the previous live value. If insertion into the index throws, the newly inserted list node is rolled back. Expiration cleanup may already have removed stale records. Hash/equality and destructors must be nonthrowing; the implementation does not attempt recovery from those contract violations.

## Why no expiration heap?

A heap adds stale records on each update unless handles or generations are maintained. Scanning the bounded list trades write latency for a compact ownership model and strict space proportional to capacity. A future heap-based variant would need measured write-heavy workloads and explicit limits on tombstones.

## Metrics

Hits/misses count get lookups. Evictions count live capacity victims; expirations count removed stale entries. Explicit erasure and invalidation do not increment either eviction or expiration. Metrics are cumulative, mutex-protected snapshots.
