#ifndef _ANGEL_TIMERTASK
#define _ANGEL_TIMERTASK

#include <cinttypes>
#include <functional>

namespace Angel {

typedef std::function<void()> TimerCallback;

class TimerTask {
public:
    int64_t timeout() const { return _timeout; }
    int64_t interval() const { return _interval; }
    const TimerCallback timerCb() const { return _timerCb; }
    void setTimeout(int64_t timeout) { _timeout = timeout; }
    void setInterval(int64_t interval) { _interval = interval; }
    void setTimerCb(const TimerCallback _cb) 
    { _timerCb = _cb; }
private:
    int64_t _timeout = 0;
    int64_t _interval = 0;
    TimerCallback _timerCb;
};
}

#endif // _ANGEL_TIMERTASK
