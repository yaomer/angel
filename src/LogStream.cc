#include <iostream>
#include <sstream>
#include <thread>
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

    thread_local const char *log_current_tid = getThreadIdStr();
    thread_local size_t log_current_tid_len = strlen(log_current_tid);

    __thread char log_time_buf[32];
    __thread time_t log_last_second = 0;

    const char *formatTime()
    {
        struct tm tm;
        long long ms = TimeStamp::now();
        time_t seconds = ms / 1000;

        if (seconds != log_last_second) {
            gmtime_r(&seconds, &tm);
            tm.tm_hour += 8;
            snprintf(log_time_buf, sizeof(log_time_buf),
                    "%4d-%02d-%02d %02d:%02d:%02d.%03lld",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec, ms % 1000);
            log_last_second = seconds;
        } else {
            snprintf(log_time_buf + 21, sizeof(log_time_buf) - 21,
                    "%03lld", ms % 1000);
        }
        return log_time_buf;
    }

    __thread char log_output_buf[65536];

    void output(int level, const char *file, int line,
            const char *func, const char *fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        char *ptr = log_output_buf;
        char *eptr = log_output_buf + sizeof(log_output_buf);
        size_t len = 0;
        memcpy(ptr, levelStr[level], 7);
        ptr += 7;
        memcpy(ptr, formatTime(), 23);
        ptr += 23;
        *ptr++ = ' ';
        len = strlen(func);
        memcpy(ptr, func, len);
        ptr += len;
        memcpy(ptr, ": ", 2);
        ptr += 2;
        vsnprintf(ptr, eptr - 256 - ptr, fmt, ap);
        ptr += strlen(ptr);
        memcpy(ptr, " - ", 3);
        ptr += 3;
        len = strlen(file);
        memcpy(ptr, file, len);
        ptr += len;
        *ptr++ = ':';
        snprintf(ptr, eptr - ptr, "%d", line);
        ptr += strlen(ptr);
        memcpy(ptr, " - ", 3);
        ptr += 3;
        memcpy(ptr, log_current_tid, log_current_tid_len);
        ptr += log_current_tid_len;
        *ptr++ = '\n';
        va_end(ap);
        __logger.writeToBuffer(log_output_buf, ptr - log_output_buf);
        if (level == LogStream::FATAL)
            __logger.quit();
    }
}

// LogStream::LogStream(int level, const char *file, int line, const char *func)
//     : _level(level),
//     _file(file),
//     _line(line),
//     _func(func)
// {
//     *this << levelStr[level]
//           << TimeStamp::timeStr(TimeStamp::LOCAL_TIME)
//           << " " << _func << ": ";
// }
//
// LogStream::~LogStream()
// {
//     *this << " - " << _file << ":" << _line
//           << " - " << getThreadIdStr()
//           << "\n";
//     __logger.writeToBuffer(_buffer.peek(), _buffer.readable());
//     if (_level == FATAL)
//         __logger.quit();
// }
//
// static const char digits[] = "9876543210123456789";
// static const char* zero = digits + 9;
//
// namespace Angel {
//
//     __thread char _itoa_buf[64];
// }
//
// // value -> str
// template<typename T>
// const char *convert(T value)
// {
//     T n = value;
//     char *p = _itoa_buf;
//
//     do {
//         int lsd = static_cast<int>(n % 10);
//         n /= 10;
//         *p++ = zero[lsd];
//     } while (n);
//     if (value < 0)
//         *p++ = '-';
//     *p = '\0';
//
//     std::reverse(_itoa_buf, p);
//
//     return _itoa_buf;
// }
//
// template <typename V>
// void numberToStr(LogStream& stream, V v)
// {
//     stream << convert(v);
// }
//
// LogStream& LogStream::operator<<(const char *s)
// {
//     if (s)
//         _buffer.append(s, std::strlen(s));
//     else
//         _buffer.append("(null)", 6);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(const std::string& s)
// {
//     _buffer.append(s.data(), s.size());
//     return *this;
// }
//
// LogStream& LogStream::operator<<(char v)
// {
//     _buffer.append(&v, 1);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(short v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(unsigned short v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(int v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(unsigned int v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(long v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(unsigned long v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(long long v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(unsigned long long v)
// {
//     numberToStr(*this, v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(float v)
// {
//     *this << static_cast<double>(v);
//     return *this;
// }
//
// LogStream& LogStream::operator<<(double v)
// {
//     char buf[32];
//     snprintf(buf, sizeof(buf), "%.8g", v);
//     *this << buf;
//     return *this;
// }

void Angel::setLoggerLevel(int level)
{
    __loggerLevel = level;
}

size_t Angel::getThreadId()
{
    return std::hash<std::thread::id>()(std::this_thread::get_id());
}

namespace Angel {

    __thread char _tid_buf[32];
}

const char *Angel::getThreadIdStr()
{
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    strncpy(_tid_buf, oss.str().c_str(), sizeof(_tid_buf));
    return _tid_buf;
}
