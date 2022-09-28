#ifndef _ANGEL_UTIL_H
#define _ANGEL_UTIL_H

#include <thread>
#include <vector>

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

void daemon();

#define UNUSED(x) ((void)(x))

}
}

#endif // _ANGEL_UTIL_H
