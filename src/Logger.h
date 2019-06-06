#ifndef _ANGEL_LOGGER_H
#define _ANGEL_LOGGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "Buffer.h"
#include "LogStream.h"

namespace Angel {

class Logger {
public:
    Logger();
    ~Logger();
    const size_t _writeBufMaxSize = 1024 * 1024;
    void writeToBuffer(const std::string& s);
    void writeToBufferUnlocked(const std::string& s);
    void wakeup();
    void writeToFile();
    void flushToFile();
    void waitFor();
    void quit() 
    { 
        _quit = true; 
        wakeup();
    }
private:
    Buffer _writeBuf;
    Buffer _flushBuf;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _condVar;
    std::atomic_bool _quit;
    int _fd;
};

extern Angel::Logger _logger;

const char *strerrno();
const char *strerr(int err);

}

#endif // _ANGEL_LOGGER_H
