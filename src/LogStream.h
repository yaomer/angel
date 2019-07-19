#ifndef _ANGEL_LOGSTREAM_H
#define _ANGEL_LOGSTREAM_H

#include <string>
#include "Buffer.h"
#include "Logger.h"

namespace Angel {

void output(int level, const char *file, int line,
        const char *func, const char *fmt, ...);

extern int __loggerLevel;

// 设置打印的日志级别
// 如果设置为WARN，则只打印>=WARN级别的日志
void setLoggerLevel(int level);

size_t getThreadId();
const char *getThreadIdStr();

}

#define logDebug(...) \
    if (Angel::Logger::DEBUG >= Angel::__loggerLevel) \
        Angel::output(Angel::Logger::DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define logInfo(...) \
    if (Angel::Logger::INFO >= Angel::__loggerLevel) \
        Angel::output(Angel::Logger::INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define logWarn(...) \
    if (Angel::Logger::WARN >= Angel::__loggerLevel) \
        Angel::output(Angel::Logger::WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define logError(...) \
    if (Angel::Logger::ERROR >= Angel::__loggerLevel) \
        Angel::output(Angel::Logger::ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define logFatal(...) \
    if (Angel::Logger::FATAL >= Angel::__loggerLevel) \
        Angel::output(Angel::Logger::FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif // _ANGEL_LOGSTREAM_H
