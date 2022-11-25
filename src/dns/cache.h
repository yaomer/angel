#ifndef __ANGEL_DNS_CACHE_H
#define __ANGEL_DNS_CACHE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

#include <angel/util.h>

namespace angel {
namespace dns {

struct cache_item {
    std::vector<std::string> addr_list;
    int64_t expire;
};

// Only Cache A record
class cache {
public:
    cache_item *get(std::string_view name)
    {
        std::lock_guard<std::mutex> lk(mtx);
        auto it = cache_map.find(std::string(name));
        if (it != cache_map.end()) return &it->second;
        return nullptr;
    }
    void put(std::string_view name, const std::vector<std::string>& addr_list, uint32_t ttl)
    {
        cache_item a;
        a.addr_list = addr_list;
        a.expire = angel::util::get_cur_time_ms() + ttl * 1000;
        {
            std::lock_guard<std::mutex> lk(mtx);
            cache_map.emplace(name, std::move(a));
        }
    }
    void evict()
    {
        auto now = angel::util::get_cur_time_ms();
        std::lock_guard<std::mutex> lk(mtx);
        for (auto it = cache_map.begin(); it != cache_map.end(); ) {
            if (it->second.expire <= now) {
                auto del_iter = it++;
                cache_map.erase(del_iter);
            } else {
                ++it;
            }
        }
    }
private:
    std::unordered_map<std::string, cache_item> cache_map;
    // TODO: Concurrency-Hash
    std::mutex mtx;
};

}
}

#endif // __ANGEL_DNS_CACHE_H
