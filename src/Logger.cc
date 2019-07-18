#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <csignal>
#include "TimeStamp.h"
#include "Logger.h"
#include "LogStream.h"

// 日志线程两种情况下会被唤醒：
// 1. _writeBuf.readable() > _writeBufMaxSize
// 2. 不管1.是否满足，每隔1s唤醒一次

using namespace Angel;

namespace Angel {
    Angel::Logger __logger;

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
}

void flush_log_buf(int signo)
{
    __logger.quit();
}

Logger::Logger()
    : _thread(std::thread([this]{ this->threadFunc(); })),
    _quit(false),
    _flag(FLUSH_TO_FILE),
    _fd(-1),
    _filesize(0)
{
    mkdir(".log", 0777);
    signal(SIGINT, flush_log_buf);
    logInfo("[Logger::ctor]");
}

Logger::~Logger()
{
    logInfo("[Logger::dtor]");
    quit();
    _thread.join();
}

void Logger::setFilename()
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
    setFilename();
    _fd = open(_filename.c_str(),
            O_WRONLY | O_APPEND | O_CREAT, 0664);
    if (_fd < 0) {
        logWarn("can't open %s: %s, now write to stdout",
                _filename.c_str(), strerrno());
        _flag = FLUSH_TO_STDOUT;
        _fd = _flag;
        return;
    }
    _filesize = 0;
}

void Logger::rollFile()
{
    if (_filesize >= _ROLLFILE_MAX_SIZE) {
        ::close(_fd);
        creatFile();
    }
}

void Logger::setFlush()
{
    switch (_flag) {
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
            _condVar.wait_for(mlock, std::chrono::seconds(1));
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
    write(_fd, _flushBuf.peek(), _flushBuf.readable());
    _filesize += _flushBuf.readable();
    _flushBuf.retrieveAll();
    if (_flag == FLUSH_TO_FILE)
        rollFile();
}

void Logger::writeToBuffer(const char *s, size_t len)
{
    std::lock_guard<std::mutex> mlock(_mutex);
    _writeBuf.append(s, len);
    if (_writeBuf.readable() > _writeBufMaxSize)
        wakeup();
}

void Logger::wakeup()
{
    _condVar.notify_one();
}

void Logger::flushToStdout()
{
    __logger._flag = FLUSH_TO_STDOUT;
}

void Logger::flushToStderr()
{
    __logger._flag = FLUSH_TO_STDERR;
}
