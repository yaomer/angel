#ifndef _ANGEL_TIMERTASK_H
#define _ANGEL_TIMERTASK_H

#include <cinttypes>
#include <functional>
#include "decls.h"

namespace Angel {

class TimerTask {
public:
    TimerTask(int64_t timeout, int64_t interval, const TimerCallback _cb)
        : _timeout(timeout),
        _interval(interval),
        _timerCb(_cb) {  }
    size_t id() const { return _id; }
    int64_t timeout() const { return _timeout; }
    int64_t interval() const { return _interval; }
    const TimerCallback timerCb() const { return _timerCb; }
    void setId(size_t id) { _id = id; } 
    void setTimeout(int64_t timeout) { _timeout = timeout; }
    void setInterval(int64_t interval) { _interval = interval; }
    void setTimerCb(const TimerCallback _cb) 
    { _timerCb = std::move(_cb); }
private:
    size_t _id = 0;
    int64_t _timeout = 0;
    int64_t _interval = 0;
    TimerCallback _timerCb;
};
}

#endif // _ANGEL_TIMERTASK_H
