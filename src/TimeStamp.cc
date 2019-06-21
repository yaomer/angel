#include <time.h>
#include <sys/time.h>
#include <iostream>
#include "TimeStamp.h"

using namespace Angel;

int64_t TimeStamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

namespace Angel {

    __thread char _timestr_buf[32];
}

// len >= 25
const char *TimeStamp::timeStr(int option)
{
    struct tm tm;
    int64_t ms = now();
    time_t seconds = ms / 1000;

    gmtime_r(&seconds, &tm);
    if (option == LOCAL_TIME)
        tm.tm_hour += 8;
    snprintf(_timestr_buf, sizeof(_timestr_buf),
            "%4d-%02d-%02d %02d:%02d:%02d.%04lld",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, ms % 1000);
    return _timestr_buf;
}
