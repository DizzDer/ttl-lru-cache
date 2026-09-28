#include "ttl_lru_cache.hpp"
#include <iostream>
#include <string>

int main() {
    using namespace std::chrono_literals;
    caching::ttl_lru_cache<std::string, std::string> cache(2);
    cache.put("profile:42", "Ada", 5min);
    cache.put("profile:7", "Grace", 5min);
    std::cout << cache.get("profile:42").value_or("missing") << '\n';
    cache.put("profile:9", "Linus", 5min);
    std::cout << "profile:7 = " << cache.get("profile:7").value_or("evicted") << '\n';
    const auto stats = cache.stats();
    std::cout << "hits=" << stats.hits << " misses=" << stats.misses
              << " evictions=" << stats.evictions << '\n';
}
