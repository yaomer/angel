#ifndef _ANGEL_TIMER_H
#define _ANGEL_TIMER_H

#include <inttypes.h>
#include <set>
#include <unordered_map>
#include "TimeStamp.h"
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

// 表示一个定时事件
class TimerTask {
public:
    TimerTask(int64_t expire, int64_t interval, const TimerCallback _cb)
        : _id(0),
        _expire(expire),
        _interval(interval),
        _timerCb(_cb),
        _isCancel(false)
    {  
    }
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
    bool isCancel() const { return _isCancel; }
    void canceled() { _isCancel = true; }
private:
    size_t _id;
    // 定时器的到期时间
    int64_t _expire;
    // 间隔定时器的间隔时间
    int64_t _interval;
    TimerCallback _timerCb;
    bool _isCancel;
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

// Timer使用红黑树来管理所有定时事件，这在添加和删除上
// 都可以做到足够高效。最小堆和红黑树的存取效率应该是相差无几的，
// 至于时间轮，这里我们不需要那么复杂的Timer
//
// Timer一般有两种驱动方法：
// 一种是SIGALRM信号，但在多线程环境下我们一般避免去处理信号
// 另一种就是I/O多路复用的超时参数了，这也是我们这里所使用的
// 其实在linux下，还用第三种方法，即timerfd
class Timer : noncopyable {
public:
    explicit Timer(EventLoop *loop)
        : _loop(loop),
        _timerId(1)
    {
    }
    ~Timer() {  };
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
    
    // 这里之所以要用到TimerIterator，是因为multiset提供两个erase()，
    // 一个以值为参数，一个以迭代器为参数，如果存有重复的元素，则前者
    // 会删掉所有值相同的元素，而后者只会删掉迭代器指向的那个元素，
    // 很显然我们想要的是后者
    typedef std::multiset<std::shared_ptr<TimerTask>, TimerTaskCmp>::iterator TimerIterator;
    void delTimer(const TimerIterator it, size_t id);

    EventLoop *_loop;
    // TimerTask中的expire可能会有重复
    std::multiset<std::shared_ptr<TimerTask>, TimerTaskCmp> _timer;
    std::unordered_map<size_t, std::shared_ptr<TimerTask>> _idMaps;
    // 唯一标识一个定时器，是全局递增的
    size_t _timerId;
};
}

#endif // _ANGEL_TIMERTASK_H
