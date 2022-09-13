#ifndef _ANGEL_UTIL_H
#define _ANGEL_UTIL_H

#include <thread>
#include <vector>

namespace angel {

namespace util {

const char *strerrno();
const char *strerr(int err);

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

std::string base64_encode(const char *data, size_t len);
std::string sha1(const std::string& data, bool normal);

void daemon();

#define UNUSED(x) ((void)(x))

}
}

#endif // _ANGEL_UTIL_H
