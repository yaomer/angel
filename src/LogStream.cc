#include <iostream>
#include "TimeStamp.h"
#include "LogStream.h"

using namespace Angel;

namespace Angel {

// 默认打印所有级别的日志
int __loggerLevel = LogStream::DEBUG;

const char *levelStr[LogStream::LEVEL_NUMS] = {
    "DEBUG: ",
    "INFO:  ",
    "WARN:  ",
    "ERROR: ",
    "FATAL: ",
};
}

LogStream::LogStream(int level, const char *file, int line, const char *func)
    : _level(level),
    _file(file),
    _line(line),
    _func(func)
{
    *this << levelStr[level]
          << TimeStamp::timeStr(TimeStamp::LOCAL_TIME)
          // << " " << std::this_thread::get_id()
          << " " << _func << ": ";
}

LogStream::~LogStream()
{
    *this << " - " << _file << ":" << _line << "\n";
    __logger.writeToBufferUnlocked(_buffer.c_str());
    if (_level == FATAL)
        __logger.quit();
}

const char digits[] = "9876543210123456789";
const char* zero = digits + 9;

template<typename T>
size_t convert(char *buf, T value)
{
    T n = value;
    char *p = buf;

    do {
        int lsd = static_cast<int>(n % 10);
        n /= 10;
        *p++ = zero[lsd];
    } while (n);
    if (value < 0)
        *p++ = '-';
    *p = '\0';

    std::reverse(buf, p);

    return p - buf;
}

template <typename V>
void numberToStr(LogStream& stream, V v)
{
    char buf[32];
    convert(buf, v);
    stream << buf;
}

LogStream& LogStream::operator<<(const char *s)
{
    if (s)
        _buffer.append(s, std::strlen(s));
    else
        _buffer.append("(null)", 6);
    return *this;
}

LogStream& LogStream::operator<<(const std::string& s)
{
    _buffer.append(s.data(), s.size());
    return *this;
}

LogStream& LogStream::operator<<(char v)
{
    _buffer.append(&v, 1);
    return *this;
}

LogStream& LogStream::operator<<(short v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned short v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(int v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned int v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(long v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(long long v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long long v)
{
    numberToStr(*this, v);
    return *this;
}

LogStream& LogStream::operator<<(float v)
{
    *this << static_cast<double>(v);
    return *this;
}

LogStream& LogStream::operator<<(double v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.8g", v);
    *this << buf;
    return *this;
}

void Angel::setLoggerLevel(int level)
{
    __loggerLevel = level;
}
