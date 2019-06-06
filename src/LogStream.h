#ifndef _ANGEL_LOGSTREAM_H
#define _ANGEL_LOGSTREAM_H

#include <string>
#include "Buffer.h"
#include "Logger.h"

namespace Angel {

class LogStream {
public:
    LogStream(int level, const char *file, int line, const char *func);
    ~LogStream();
    enum LEVEL {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        LEVEL_NUMS,
    };
    LogStream& operator<<(const char *s);
    LogStream& operator<<(const std::string& s);
    LogStream& operator<<(char v);
    LogStream& operator<<(short v);
    LogStream& operator<<(unsigned short v);
    LogStream& operator<<(int v);
    LogStream& operator<<(unsigned int v);
    LogStream& operator<<(long v);
    LogStream& operator<<(unsigned long v);
    LogStream& operator<<(long long v);
    LogStream& operator<<(unsigned long long v);
    LogStream& operator<<(float v);
    LogStream& operator<<(double v);
private:
    Buffer _buffer;
    int _level;
    const char *_file;
    int _line;
    const char *_func;
};

extern int loggerLevel;

// 设置打印的日志级别
// 如果设置为WARN，则只打印>=WARN级别的日志
void setLoggerLevel(int level);

}

// 每次调用LOG_XXX都会构造一个匿名LogStream对象，该对象只在
// 此次调用中有效，从而保证了线程安全
// 当此次调用结束时，它就会被析构，所写的数据会被写入到Logger中，
// Logger则会在合适的时机将数据flush到文件中
#define LOG_DEBUG \
    if (Angel::LogStream::DEBUG >= Angel::loggerLevel) \
        Angel::LogStream(Angel::LogStream::DEBUG, __FILE__, __LINE__, __func__)
#define LOG_INFO \
    if (Angel::LogStream::INFO >= Angel::loggerLevel) \
        Angel::LogStream(Angel::LogStream::INFO, __FILE__, __LINE__, __func__)
#define LOG_WARN \
    if (Angel::LogStream::WARN >= Angel::loggerLevel) \
        Angel::LogStream(Angel::LogStream::WARN, __FILE__, __LINE__, __func__)
#define LOG_ERROR \
    if (Angel::LogStream::ERROR >= Angel::loggerLevel) \
        Angel::LogStream(Angel::LogStream::ERROR, __FILE__, __LINE__, __func__)
#define LOG_FATAL \
    if (Angel::LogStream::FATAL >= Angel::loggerLevel) \
        Angel::LogStream(Angel::LogStream::FATAL, __FILE__, __LINE__, __func__)

#endif // _ANGEL_LOGSTREAM_H
