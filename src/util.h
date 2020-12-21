#ifndef _ANGEL_UTIL_H
#define _ANGEL_UTIL_H

#include <thread>

namespace angel {

namespace util {

const char *strerrno();
const char *strerr(int err);

size_t get_cur_thread_id();
std::string get_cur_thread_id_str();

int64_t get_cur_time_ms();
int64_t get_cur_time_us();

int get_ncpus();
void set_thread_affinity(pthread_t tid, int cpu_number);

void daemon();

#define UNUSED(x) ((void)(x))

}
}

#endif // _ANGEL_UTIL_H
