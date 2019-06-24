#ifndef _ANGEL_TIMER_H
#define _ANGEL_TIMER_H

#include <cinttypes>
#include <set>
#include "TimerTask.h"
#include "TimeStamp.h"
#include "noncopyable.h"

namespace Angel {

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

#endif // _ANGEL_TIMER_H
