#include <string.h>
#include <errno.h>
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
}
