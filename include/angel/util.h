#ifndef _ANGEL_UTIL_H
#define _ANGEL_UTIL_H

#include <vector>
#include <algorithm>
#include <thread>

namespace angel {
namespace util {

// thread-safe strerror()
const char *strerr(int err);
// strerr(errno)
const char *strerrno();

// if c == ','
// for [a,b,c,d] return [a][b][c][d]
std::vector<std::string>
split(const char *s, const char *es, char c);

size_t get_cur_thread_id();
std::string get_cur_thread_id_str();

int64_t get_cur_time_ms();
int64_t get_cur_time_us();

int get_ncpus();
void set_thread_affinity(pthread_t tid, int cpu_number);

std::string base64_encode(std::string_view data);
std::string base64_decode(std::string_view data);

std::string sha1(std::string_view data, bool normal);

// check ipv4 addr format
bool check_ip(std::string_view ipv4_addr);

// configuration file syntax:
// 1) field and value are separated by spaces
// 2) lines beginning with # as comments
typedef std::vector<std::string> ConfigParam;
typedef std::vector<ConfigParam> ConfigParamlist;
ConfigParamlist parse_conf(const char *pathname);

bool write_file(int fd, const char *buf, size_t len);
bool copy_file(const std::string& from, const std::string& to);

bool is_regular_file(const std::string& path);
bool is_directory(const std::string& path);

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

void daemon();

#define UNUSED(x) ((void)(x))

}
}

#endif // _ANGEL_UTIL_H
