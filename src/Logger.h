#ifndef _ANGEL_LOGGER_H
#define _ANGEL_LOGGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <atomic>
#include "Buffer.h"
#include "LogStream.h"
#include "noncopyable.h"

namespace Angel {

class Logger : noncopyable {
public:
    Logger();
    ~Logger();
    enum FLAGS {
        FLUSH_TO_FILE = 1,
        FLUSH_TO_STDOUT,
        FLUSH_TO_STDERR,
    };
    const size_t _writeBufMaxSize = 1024 * 1024;
    void writeToBuffer(const char *s, size_t len);
    void wakeup();
    void quit()
    { _quit = true; wakeup(); }
    static void flushToStdout();
    static void flushToStderr();
private:
    void flush();
    void setFlush();
    void threadFunc();
    void setFilename();
    void creatFile();
    void rollFile();

    static const size_t _ROLLFILE_MAX_SIZE = 1024 * 1024 * 1024;

    Buffer _writeBuf;
    Buffer _flushBuf;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _condVar;
    std::atomic_bool _quit;
    int _flag;
    int _fd;
    std::string _filename;
    size_t _filesize;
};

extern Angel::Logger __logger;

const char *strerrno();
const char *strerr(int err);

}

#endif // _ANGEL_LOGGER_H
