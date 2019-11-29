#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <sstream>
#include <thread>

namespace Angel {

    thread_local char _errnobuf[256];

    // 线程安全的strerror
    const char *strerr(int err)
    {
        strerror_r(err, _errnobuf, sizeof(_errnobuf));
        return _errnobuf;
    }
    const char *strerrno()
    {
        return strerr(errno);
    }

    size_t getThreadId()
    {
        return std::hash<std::thread::id>()(std::this_thread::get_id());
    }

    thread_local char _tid_buf[32];

    const char *getThreadIdStr()
    {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        strncpy(_tid_buf, oss.str().c_str(), sizeof(_tid_buf));
        return _tid_buf;
    }

    int64_t nowMs()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }

    int64_t nowUs()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return tv.tv_sec * 1000000 + tv.tv_usec;
    }
}
