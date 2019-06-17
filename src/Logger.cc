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
#include "Logger.h"
#include "LogStream.h"

// 日志线程两种情况下会被唤醒：
// 1. _writeBuf.readable() > _writeBufMaxSize
// 2. 不管1.是否满足，每隔1s唤醒一次

using namespace Angel;

namespace Angel {
    Angel::Logger __logger;

    __thread char _errnobuf[256];

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

Logger::Logger()
    : _thread(std::thread([this]{ this->flushToFile(); })),
    _quit(false),
    _flag(FLUSH_TO_FILE)
{
    mkdir(".log", 0777);
}

Logger::~Logger()
{
    quit();
    _thread.join();
}

void Logger::flushToFile()
{
    switch (_flag) {
    case FLUSH_TO_FILE:
        _fd = open("./.log/x.log", O_WRONLY | O_APPEND | O_CREAT, 0777);
        break;
    case FLUSH_TO_STDOUT:
        _fd = STDOUT_FILENO;
        break;
    case FLUSH_TO_STDERR:
        _fd = STDERR_FILENO;
        break;
    }

    while (1) {
        waitFor();
        if (_flushBuf.readable() > 0)
            writeToFile();
    }
}

void Logger::waitFor()
{
    std::unique_lock<std::mutex> mlock(_mutex);
    if (!_quit)
        _condVar.wait_for(mlock, std::chrono::seconds(1));
    if (_quit) {
        _writeBuf.swap(_flushBuf);
        writeToFile();
        abort();
    }
    if (_writeBuf.readable() > 0)
        _writeBuf.swap(_flushBuf);
}

void Logger::writeToFile()
{
    while (1) {
        // 一般情况下，一次肯定能写完
        ssize_t n = write(_fd, _flushBuf.peek(), _flushBuf.readable());
        if (n < 0) {
            LOG_ERROR << "write error: " << strerrno();
            _flushBuf.retrieveAll();
            return;
        }
        _flushBuf.retrieve(n);
        if (_flushBuf.readable() == 0)
            break;
    }
}

void Logger::writeToBufferUnlocked(const std::string& s)
{
    _writeBuf.append(s.data(), s.size());
    if (_writeBuf.readable() > _writeBufMaxSize)
        wakeup();
}

void Logger::writeToBuffer(const std::string& s)
{
    std::lock_guard<std::mutex> mlock(_mutex);
    writeToBufferUnlocked(s);
}

void Logger::wakeup()
{
    if (_writeBuf.readable() > 0)
        _condVar.notify_one();
}
