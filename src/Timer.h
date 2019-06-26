#ifndef _ANGEL_TIMER_H
#define _ANGEL_TIMER_H

#include <cinttypes>
#include <set>
#include "TimeStamp.h"
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class TimerTask {
public:
    TimerTask(int64_t timeout, int64_t interval, const TimerCallback _cb);
    ~TimerTask();
    size_t id() const { return _id; }
    int64_t timeout() const { return _timeout; }
    int64_t interval() const { return _interval; }
    const TimerCallback timerCb() const { return _timerCb; }
    void setId(size_t id) { _id = id; } 
    void setTimeout(int64_t timeout) { _timeout = timeout; }
    void setInterval(int64_t interval) { _interval = interval; }
    void setTimerCb(const TimerCallback _cb) 
    { _timerCb = _cb; }
private:
    size_t _id;
    int64_t _timeout;
    int64_t _interval;
    TimerCallback _timerCb;
};

class TimerTaskCmp {
public:
    bool operator()(const std::unique_ptr<TimerTask>& lhs,
                    const std::unique_ptr<TimerTask>& rhs) const
    {
        return lhs->timeout() < rhs->timeout();
    }
};

class Timer : noncopyable {
public:
    Timer();
    ~Timer();
    size_t addTask(TimerTask *_task);
    // 取出最小定时器
    const TimerTask *getTask() { return _timer.cbegin()->get(); };
    // 删除最小定时器事件
    void popTask() 
    { 
        _freeIdList.insert(getTask()->id());
        _timer.erase(_timer.begin()); 
    }
    // 返回最小超时值
    int64_t timeout()
    {
        int64_t timeval;
        if (!_timer.empty()) {
            timeval = llabs(getTask()->timeout() - TimeStamp::now());
        } else
            timeval = -1;
        return timeval == 0 ? 1 : timeval;
    }
    void cancelTask(size_t id);
    // 处理所有到期的定时事件
    void tick();
private:
    size_t getId();
    void putId(size_t id);

    // [0 : _timerId]之间的freeId
    std::set<size_t> _freeIdList;
    // TimerTask中的timeout可能会有重复
    std::multiset<std::unique_ptr<TimerTask>, TimerTaskCmp> _timer;
    // 唯一标识一个TimerTask
    size_t _timerId = 1;
};
}

#endif // _ANGEL_TIMERTASK_H
