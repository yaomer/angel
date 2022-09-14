#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include <sstream>

#include <angel/logger.h>

namespace angel {
namespace util {

static thread_local char errno_buf[256];

// 线程安全的strerror
const char *strerr(int err)
{
    strerror_r(err, errno_buf, sizeof(errno_buf));
    return errno_buf;
}

const char *strerrno()
{
    return strerr(errno);
}

std::vector<std::string>
split(const char *s, const char *es, char c)
{
    const char *p;
    std::vector<std::string> vlist;

    while (true) {
        p = std::find(s, es, c);
        if (p == es) break;
        vlist.emplace_back(s, p);
        s = p + 1;
    }
    vlist.emplace_back(s, p);
    return vlist;
}

size_t get_cur_thread_id()
{
    return std::hash<std::thread::id>()(std::this_thread::get_id());
}

std::string get_cur_thread_id_str()
{
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

int64_t get_cur_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int64_t get_cur_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

int get_ncpus()
{
    return std::thread::hardware_concurrency();
}

void set_thread_affinity(pthread_t tid, int cpu_number)
{
#if defined (__APPLE__)
    log_warn("OS X Not support to set thread affinity");
#elif defined (__linux__)
    assert(cpu_number < get_ncpus());
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_number, &mask);
    if (pthread_setaffinity_np(tid, sizeof(mask), &mask) < 0)
        log_error("pthread_setaffinity_np error: %s", strerrno());
#endif
}

}
}