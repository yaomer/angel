#ifndef _ANGEL_LOGGER_H
#define _ANGEL_LOGGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "Buffer.h"
#include "LogStream.h"
#include "Noncopyable.h"

namespace Angel {

class Logger : Noncopyable {
public:
    Logger();
    ~Logger();
    enum FLAGS {
        FLUSH_TO_FILE = 1,
        FLUSH_TO_STDOUT,
        FLUSH_TO_STDERR,
    };
    const size_t _writeBufMaxSize = 1024 * 1024;
    void writeToBuffer(const std::string& s);
    void writeToBufferUnlocked(const std::string& s);
    void wakeup();
    static void flushToStdout();
    static void flushToStderr();
    void quit() 
    { 
        _quit = true; 
        wakeup();
    }
private:
    void writeToFile();
    void flushToFile();

    Buffer _writeBuf;
    Buffer _flushBuf;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _condVar;
    std::atomic_bool _quit;
    int _flag;
    int _fd = -1;
};

extern Angel::Logger __logger;

const char *strerrno();
const char *strerr(int err);

}

#endif // _ANGEL_LOGGER_H
