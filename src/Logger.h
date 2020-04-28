#ifndef _ANGEL_LOGGER_H
#define _ANGEL_LOGGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <atomic>

#include "Buffer.h"
#include "LogStream.h"
#include "EventLoop.h"
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
    enum LEVEL {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
    };
    void writeToBuffer(const char *s, size_t len);
    void wakeup();
    void setFlush(int flush) { _logFlush = flush; }

    static void quit();

    static const size_t log_buffer_max_size = 512 * 1024;
    static const size_t log_roll_filesize = 1024 * 1024 * 1024;
    static const int log_flush_interval = 1;
private:
    void flush();
    void setFlush();
    void threadFunc();
    void getFilename();
    void creatFile();
    void rollFile();

    const char *timeStr();

    Buffer _writeBuf;
    Buffer _flushBuf;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _condVar;
    std::atomic_bool _quit;
    int _fd;
    std::string _filename;
    size_t _filesize;
    int _logFlush;
};

extern Angel::Logger __logger;

}

#endif // _ANGEL_LOGGER_H
