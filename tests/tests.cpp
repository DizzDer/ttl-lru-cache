#include "ttl_lru_cache.hpp"
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#define CHECK(x) do { if (!(x)) throw std::runtime_error("check failed: " #x); } while (false)
struct manual_clock {
    using duration = std::chrono::milliseconds;
    using time_point = std::chrono::time_point<manual_clock, duration>;
    static constexpr bool is_steady = true;
    inline static time_point current{};
    static time_point now() noexcept { return current; }
};

int main() {
    using namespace std::chrono_literals;
    try {
        bool invalid = false;
        try { caching::ttl_lru_cache<int, int> bad(0); }
        catch (const std::invalid_argument&) { invalid = true; }
        CHECK(invalid);
        caching::ttl_lru_cache<int, std::string, manual_clock> cache(2);
        cache.put(1, "one", 10ms); cache.put(2, "two", 100ms);
        CHECK(cache.get(1) == "one");
        cache.put(3, "three", 100ms); // key 2 is LRU, despite longer TTL
        CHECK(!cache.get(2)); CHECK(cache.stats().evictions == 1);
        manual_clock::current += 10ms;
        CHECK(!cache.get(1)); // exact expiration boundary
        CHECK(cache.stats().expirations == 1);
        cache.put(3, "updated", 5ms);
        CHECK(cache.get(3) == "updated");
        cache.put(3, "remove", 0ms); CHECK(!cache.get(3));
        CHECK(!cache.erase(99));
        cache.put(4, "stale", 1ms); cache.put(5, "live", 100ms);
        manual_clock::current += 1ms;
        cache.put(6, "new", 100ms);
        CHECK(cache.get(5) == "live"); // stale is purged before live eviction
        CHECK(cache.stats().evictions == 1);
        manual_clock::current += 100ms;
        CHECK(cache.purge_expired() == 2); CHECK(cache.stats().stored == 0);
        bool overflow = false;
        try { cache.put(7, "overflow", manual_clock::duration::max()); }
        catch (const std::overflow_error&) { overflow = true; }
        CHECK(overflow);

        caching::ttl_lru_cache<int, int> shared(64);
        std::vector<std::thread> workers;
        std::atomic<bool> correct{true};
        for (int t = 0; t < 8; ++t) workers.emplace_back([&, t] {
            for (int i = 0; i < 2000; ++i) {
                shared.put(t, i, 1h);
                if (shared.get(t) != i) correct = false;
            }
        });
        for (auto& worker : workers) worker.join();
        CHECK(correct); CHECK(shared.stats().stored == 8);
        CHECK(shared.stats().hits == 16000);
        std::cout << "All cache tests passed (including 16000 concurrent put/get pairs).\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
