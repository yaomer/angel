#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <iostream>
#include <chrono>

#include "TimeStamp.h"
#include "Logger.h"

using namespace Angel;

namespace Angel {

    Angel::Logger __logger;
}

// 用户键入Ctr+C后，flush logger
void flush_log_buf(int signo)
{
    Logger::quit();
    fprintf(stderr, "\n");
}

Logger::Logger()
    : _thread(std::thread([this]{ this->threadFunc(); })),
    _quit(false),
    _fd(-1),
    _filesize(0),
    _logFlush(FLUSH_TO_FILE)
{
    mkdir(".log", 0777);
    signal(SIGINT, flush_log_buf);
}

Logger::~Logger()
{
    quit();
    _thread.join();
}

void Logger::getFilename()
{
    _filename.clear();
    _filename += "./.log/";
    char *p = const_cast<char*>
        (TimeStamp::timeStr(TimeStamp::LOCAL_TIME));
    p[19] = '\0';
    _filename += p;
    _filename += ".log";
}

void Logger::creatFile()
{
    getFilename();
    _fd = open(_filename.c_str(),
            O_WRONLY | O_APPEND | O_CREAT, 0664);
    if (_fd < 0) {
        logWarn("can't open %s: %s, now write to stdout",
                _filename.c_str(), strerrno());
        _logFlush = FLUSH_TO_STDOUT;
        _fd = _logFlush;
        return;
    }
    _filesize = 0;
}

void Logger::rollFile()
{
    if (_filesize >= log_roll_filesize) {
        ::close(_fd);
        creatFile();
    }
}

void Logger::setFlush()
{
    switch (_logFlush) {
    case FLUSH_TO_FILE:
        creatFile();
        logInfo("write logger to file");
        break;
    case FLUSH_TO_STDOUT:
        _fd = STDOUT_FILENO;
        logInfo("write logger to stdout");
        break;
    case FLUSH_TO_STDERR:
        _fd = STDERR_FILENO;
        logInfo("write logger to stderr");
        break;
    }
}

void Logger::threadFunc()
{
    setFlush();
    while (1) {
        {
        std::unique_lock<std::mutex> mlock(_mutex);
        _condVar.wait_for(mlock, std::chrono::seconds(log_flush_interval));
        if (_writeBuf.readable() > 0)
            _writeBuf.swap(_flushBuf);
        }
        if (_flushBuf.readable() > 0)
            flush();
        if (_quit) {
            abort();
        }
    }
}

void Logger::flush()
{
    while (_flushBuf.readable() > 0) {
        ssize_t n = ::write(_fd, _flushBuf.peek(), _flushBuf.readable());
        if (n < 0) {
            fprintf(stderr, "write: %s", strerrno());
            break;
        }
        _filesize += n;
        _flushBuf.retrieve(n);
    }
    if (_logFlush == FLUSH_TO_FILE)
        rollFile();
}

void Logger::writeToBuffer(const char *s, size_t len)
{
    std::lock_guard<std::mutex> mlock(_mutex);
    _writeBuf.append(s, len);
    if (_writeBuf.readable() > log_buffer_max_size)
        wakeup();
}

void Logger::wakeup()
{
    _condVar.notify_one();
}

void Logger::quit()
{
    __logger._quit = true;
    __logger.wakeup();
}
