#ifndef _ANGEL_UTIL_H
#define _ANGEL_UTIL_H

#include <vector>
#include <sstream>
#include <algorithm>
#include <thread>
#include <charconv>
#include <optional>

namespace angel {
namespace util {

// thread-safe strerror()
const char *strerr(int err);
// strerr(errno)
const char *strerrno();

size_t get_cur_thread_id();
std::string get_cur_thread_id_str();

int64_t get_cur_time_ms();
int64_t get_cur_time_us();

int get_ncpus();
void set_thread_affinity(pthread_t tid, int cpu_number);

std::string base64_encode(std::string_view data);
std::string base64_decode(std::string_view data);

// check ipv4 addr format
bool check_ip(std::string_view ipv4_addr);

void daemon();

//===================================================
//=========== Some common file operations ===========
//===================================================

// configuration file syntax:
// 1) field and value are separated by spaces
// 2) lines beginning with # as comments
typedef std::vector<std::string> ConfigParam;
typedef std::vector<ConfigParam> ConfigParamlist;
ConfigParamlist parse_conf(const char *pathname);

ssize_t read_file(int fd, const char *buf, size_t len);
bool write_file(int fd, const char *buf, size_t len);
bool copy_file(const std::string& from, const std::string& to);

bool is_regular_file(const std::string& path);
bool is_directory(const std::string& path);

off_t get_file_size(const std::string& path);
off_t get_file_size(int fd);

off_t page_aligned(off_t offset);

//===================================================
//========== Some common string operations ==========
//===================================================

// If it is completely converted, return true (such as "12a" is wrong)
template <typename R, std::enable_if_t<std::is_integral_v<R>, int> = 0>
inline std::optional<R> svtointegral(std::string_view s)
{
    R val;
    auto res = std::from_chars(s.data(), s.data() + s.size(), val);
    if (res.ec == std::errc() && res.ptr == s.data() + s.size())
        return val;
    return std::nullopt;
}

inline std::optional<int> svtoi(std::string_view s)
{
    return svtointegral<int>(s);
}

inline std::optional<long> svtol(std::string_view s)
{
    return svtointegral<long>(s);
}

inline std::optional<long long> svtoll(std::string_view s)
{
    return svtointegral<long long>(s);
}

inline bool is_digits(std::string_view s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

inline std::string to_lower(std::string_view s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

inline std::string to_upper(std::string_view s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return r;
}

inline bool equal_case(std::string_view s1, std::string_view s2)
{
    return s1.size() == s2.size() && strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
}

inline bool starts_with(std::string_view s, std::string_view prefix)
{
    return s.size() >= prefix.size() && memcmp(s.data(), prefix.data(), prefix.size()) == 0;
}

inline bool starts_with_case(std::string_view s, std::string_view prefix)
{
    return s.size() >= prefix.size() && strncasecmp(s.data(), prefix.data(), prefix.size()) == 0;
}

inline bool ends_with(std::string_view s, std::string_view suffix)
{
    return s.size() >= suffix.size() &&
           memcmp(s.data() + s.size() - suffix.size(), suffix.data(), suffix.size()) == 0;
}

inline bool ends_with_case(std::string_view s, std::string_view suffix)
{
    return s.size() >= suffix.size() &&
           strncasecmp(s.data() + s.size() - suffix.size(), suffix.data(), suffix.size()) == 0;
}

// if c == ','
// for [a,b,c,d] return [a][b][c][d]
inline std::vector<std::string_view> split(std::string_view s, int c)
{
    std::vector<std::string_view> res;
    auto p = s.begin(), end = s.end();

    while (true) {
        auto sep = std::find(p, end, c);
        res.emplace_back(p, sep - p);
        if (sep == end) break;
        p = sep + 1;
    }
    return res;
}

// Removed any leading and trailing whitespace.
inline std::string_view trim(std::string_view s)
{
    auto left = std::find_if_not(s.begin(), s.end(), ::isspace);
    if (left == s.end()) return "";

    auto right = std::find_if_not(s.rbegin(), s.rend(), ::isspace);

    auto pos = left - s.begin();
    auto n = (s.rend() - right) - pos;
    return s.substr(pos, n);
}

template<typename Container>
std::string join(const std::string& delimiter, const Container& c)
{
    if(std::begin(c) == std::end(c)) return "";

    std::ostringstream oss;

    auto last = std::begin(c);
    std::advance(last, std::size(c) - 1);

    using element_type = typename std::iterator_traits<decltype(std::begin(c))>::value_type;
    std::copy(std::begin(c), last, std::ostream_iterator<element_type>(oss, delimiter.c_str()));
    oss << *last;

    return oss.str();
}

inline std::string catviews(std::initializer_list<std::string_view> views)
{
    std::string buf;

    size_t size = 0;
    for (const auto& s : views) {
        size += s.size();
    }

    buf.resize(size);
    char *after = buf.data();
    for (const auto& s : views) {
        memcpy(after, s.data(), s.size());
        after += s.size();
    }
    return buf;
}

// Convenient fast concat const char*
template <typename... Args>
inline std::string concat(std::string_view s, Args... args)
{
    return catviews({s, args...});
}

#define UNUSED(x) ((void)(x))

}
}

#endif // _ANGEL_UTIL_H
