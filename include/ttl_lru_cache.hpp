#pragma once

#include <chrono>
#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace caching {

// A synchronized, capacity-bounded cache. Clock is injectable for deterministic
// tests; production defaults to steady_clock, which cannot jump backwards.
template<class Key, class Value, class Clock = std::chrono::steady_clock>
class ttl_lru_cache {
    using time_point = typename Clock::time_point;
    struct entry { Key key; Value value; time_point expires; };
    using list_type = std::list<entry>;
public:
    using duration = typename Clock::duration;
    struct statistics { std::size_t hits, misses, evictions, expirations, stored; };

    explicit ttl_lru_cache(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("capacity must be positive");
    }

    // Nonpositive TTL invalidates an existing key. put scans for stale entries
    // before evicting a live LRU entry; consequently put is O(capacity).
    void put(Key key, Value value, duration ttl) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ttl <= duration::zero()) { erase_unlocked(key); return; }
        const auto now = Clock::now();
        if (now > time_point::max() - ttl)
            throw std::overflow_error("TTL exceeds clock range");
        purge_unlocked(now);
        const auto found = index_.find(key);
        // Construct replacement first: a throwing Value constructor leaves the
        // old live value intact. Existing iterators survive list insertions.
        entries_.push_front(entry{std::move(key), std::move(value), now + ttl});
        if (found != index_.end()) {
            entries_.erase(found->second);
            found->second = entries_.begin();
        } else {
            try { index_.emplace(entries_.front().key, entries_.begin()); }
            catch (...) { entries_.pop_front(); throw; }
        }
        if (entries_.size() > capacity_) {
            index_.erase(entries_.back().key);
            entries_.pop_back();
            ++evictions_;
        }
    }

    // Returns a copy, never a reference whose lifetime could race with eviction.
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = index_.find(key);
        if (found == index_.end()) { ++misses_; return std::nullopt; }
        if (found->second->expires <= Clock::now()) {
            entries_.erase(found->second);
            index_.erase(found);
            ++expirations_; ++misses_;
            return std::nullopt;
        }
        std::optional<Value> result(found->second->value);
        entries_.splice(entries_.begin(), entries_, found->second);
        ++hits_;
        return result;
    }

    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return erase_unlocked(key);
    }

    std::size_t purge_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        return purge_unlocked(Clock::now());
    }

    statistics stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        // stored includes expired entries not yet visited by get/put/purge.
        return {hits_, misses_, evictions_, expirations_, entries_.size()};
    }

private:
    bool erase_unlocked(const Key& key) {
        const auto found = index_.find(key);
        if (found == index_.end()) return false;
        entries_.erase(found->second);
        index_.erase(found);
        return true;
    }
    std::size_t purge_unlocked(time_point now) {
        std::size_t count = 0;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->expires <= now) {
                index_.erase(it->key);
                it = entries_.erase(it);
                ++count;
            } else ++it;
        }
        expirations_ += count;
        return count;
    }

    const std::size_t capacity_;
    mutable std::mutex mutex_;
    list_type entries_;
    std::unordered_map<Key, typename list_type::iterator> index_;
    std::size_t hits_ = 0, misses_ = 0, evictions_ = 0, expirations_ = 0;
};

} // namespace caching
