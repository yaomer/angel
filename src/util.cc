#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sstream>

#include "util.h"
#include "logger.h"

using namespace angel;

namespace angel {
    namespace util {

        static thread_local char errno_buf[256];
    }
}

// 线程安全的strerror
const char *util::strerr(int err)
{
    strerror_r(err, errno_buf, sizeof(errno_buf));
    return errno_buf;
}

const char *util::strerrno()
{
    return strerr(errno);
}

size_t util::get_cur_thread_id()
{
    return std::hash<std::thread::id>()(std::this_thread::get_id());
}

std::string util::get_cur_thread_id_str()
{
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

int64_t util::get_cur_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int64_t util::get_cur_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

int util::get_ncpus()
{
    return std::thread::hardware_concurrency();
}

void util::set_thread_affinity(pthread_t tid, int cpu_number)
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

void util::daemon()
{
    pid_t pid;

    if ((pid = fork()) < 0) {
        log_fatal("fork error: %s", strerrno());
    }
    if (pid > 0)
        exit(0);

    if (setsid() < 0) {
        log_fatal("setsid error: %s", strerrno());
    }
    umask(0);

    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        log_fatal("open /dev/null error: %s", strerrno());
    }

    if (dup2(fd, STDIN_FILENO) < 0) {
        log_fatal("dup2 stdin error: %s", strerrno());
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        log_fatal("dup2 stdout error: %s", strerrno());
    }
    if (dup2(fd, STDERR_FILENO) < 0) {
        log_fatal("dup2 stderr error: %s", strerrno());
    }

    if (fd > STDERR_FILENO)
        close(fd);
}
