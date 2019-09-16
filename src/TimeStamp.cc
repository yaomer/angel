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

int64_t TimeStamp::nowUs()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

namespace Angel {

    thread_local char tsp_timestr_buf[32];
}

// len >= 25
const char *TimeStamp::timeStr(int option)
{
    struct tm tm;
    long long ms = now();
    time_t seconds = ms / 1000;

    gmtime_r(&seconds, &tm);
    if (option == LOCAL_TIME)
        tm.tm_hour += 8;
    snprintf(tsp_timestr_buf, sizeof(tsp_timestr_buf),
            "%4d-%02d-%02d %02d:%02d:%02d.%03lld",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, ms % 1000);
    return tsp_timestr_buf;
}
