#ifndef __ANGEL_INSENSITIVE_UNORDERED_MAP_H
#define __ANGEL_INSENSITIVE_UNORDERED_MAP_H

#include <unordered_map>

#include <angel/util.h>

namespace angel {
namespace detail {

struct case_insensitive_key {
    case_insensitive_key(const char *s) : key(s) {  }
    case_insensitive_key(std::string_view s) : key(s) {  }
    bool operator==(const case_insensitive_key& rhs) const
    {
        return angel::util::equal_case(key, rhs.key);
    }
    std::string key;
};

}

template <typename Value>
using insensitive_unordered_map = std::unordered_map<detail::case_insensitive_key, Value>;

}

namespace std {
    template<> struct hash<angel::detail::case_insensitive_key> {
        std::size_t operator()(const angel::detail::case_insensitive_key &c) const {
            return std::hash<std::string>{}(angel::util::to_lower(c.key));
        }
    };
}

#endif // __ANGEL_INSENSITIVE_UNORDERED_MAP_H
