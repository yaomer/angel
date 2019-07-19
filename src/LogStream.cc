#include <iostream>
#include "TimeStamp.h"
#include "LogStream.h"

using namespace Angel;

namespace Angel {

    // 默认打印所有级别的日志
    int __loggerLevel = Logger::DEBUG;

    const char *levelStr[Logger::LEVEL_NUMS] = {
        "DEBUG: ",
        "INFO:  ",
        "WARN:  ",
        "ERROR: ",
        "FATAL: ",
    };

    thread_local const char *log_current_tid = getThreadIdStr();
    thread_local size_t log_current_tid_len = strlen(log_current_tid);

    thread_local char log_time_buf[32];
    thread_local time_t log_last_second = 0;

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
            // 在1s内只需要格式化ms部分
            snprintf(log_time_buf + 21, sizeof(log_time_buf) - 21,
                    "%03lld", ms % 1000);
        }
        return log_time_buf;
    }

    thread_local char log_output_buf[65536];

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
        *ptr = '\0';
        va_end(ap);
        __logger.writeToBuffer(log_output_buf, ptr - log_output_buf);
        if (level == Logger::FATAL)
            __logger.quit();
    }
}

void Angel::setLoggerLevel(int level)
{
    __loggerLevel = level;
}
