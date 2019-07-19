#ifndef _ANGEL_TIMER_H
#define _ANGEL_TIMER_H

#include <inttypes.h>
#include <set>
#include <unordered_map>
#include "TimeStamp.h"
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class TimerTask {
public:
    TimerTask(int64_t expire, int64_t interval, const TimerCallback _cb)
        : _id(0),
        _expire(expire),
        _interval(interval),
        _timerCb(_cb) {  }
    ~TimerTask() {  };
    size_t id() const { return _id; }
    int64_t expire() const { return _expire; }
    int64_t interval() const { return _interval; }
    const TimerCallback timerCb() const { return _timerCb; }
    void setId(size_t id) { _id = id; } 
    void setExpire(int64_t expire) { _expire = expire; }
    void setInterval(int64_t interval) { _interval = interval; }
    void setTimerCb(const TimerCallback _cb) 
    { _timerCb = std::move(_cb); }
private:
    size_t _id;
    int64_t _expire;
    int64_t _interval;
    TimerCallback _timerCb;
};

class TimerTaskCmp {
public:
    bool operator()(const std::shared_ptr<TimerTask>& lhs,
                    const std::shared_ptr<TimerTask>& rhs) const
    {
        return lhs->expire() < rhs->expire();
    }
};

class EventLoop;

class Timer : noncopyable {
public:
    explicit Timer(EventLoop *loop);
    ~Timer();
    size_t addTimer(TimerTask *_task);
    // 返回最小超时值
    // set是可以在O(1)时间获取最小节点的，这只需要维护一个指向最小
    // 节点的指针即可，因此完全可以达到与使用最小堆相同的效果。
    int64_t timeout()
    {
        int64_t timeval;
        if (!_timer.empty()) {
            timeval = _timer.begin()->get()->expire() - TimeStamp::now();
            timeval = timeval > 0 ? timeval : 0;
        } else
            timeval = -1;
        return timeval;
    }
    void cancelTimer(size_t id);
    // 处理所有到期的定时事件
    void tick();
private:
    void addTimerInLoop(TimerTask *_task, size_t id);
    void cancelTimerInLoop(size_t id);
    
    typedef std::multiset<std::shared_ptr<TimerTask>, TimerTaskCmp>::iterator TimerIterator;
    void delTimer(const TimerIterator it, size_t id);

    EventLoop *_loop;
    // TimerTask中的expire可能会有重复
    std::multiset<std::shared_ptr<TimerTask>, TimerTaskCmp> _timer;
    std::unordered_map<size_t, std::shared_ptr<TimerTask>> _idMaps;
    size_t _timerId;
};
}

#endif // _ANGEL_TIMERTASK_H
