#ifndef _ANGEL_TIMER_H
#define _ANGEL_TIMER_H

#include <inttypes.h>
#include <set>
#include <unordered_map>

#include "noncopyable.h"
#include "util.h"

namespace angel {

typedef std::function<void()> timer_callback_t;

class timer_task_t {
public:
    timer_task_t(int64_t expire, int64_t interval, const timer_callback_t cb)
        : _id(0),
        _expire(expire),
        _interval(interval),
        _timer_cb(cb),
        _is_canceled(false)
    {
    }
    ~timer_task_t() {  };
    size_t id() const { return _id; }
    int64_t expire() const { return _expire; }
    int64_t interval() const { return _interval; }
    const timer_callback_t timer_cb() const { return _timer_cb; }
    void set_id(size_t id) { _id = id; }
    void set_expire(int64_t expire) { _expire = expire; }
    void set_interval(int64_t interval) { _interval = interval; }
    void set_timer_cb(const timer_callback_t cb)
    { _timer_cb = std::move(cb); }
    bool is_canceled() const { return _is_canceled; }
    void cancel() { _is_canceled = true; }
private:
    size_t _id;
    // 定时器的到期时间
    int64_t _expire;
    // 间隔定时器的间隔时间
    int64_t _interval;
    timer_callback_t _timer_cb;
    bool _is_canceled;
};

struct timer_task_cmp {
    bool operator()(const std::shared_ptr<timer_task_t>& lhs,
                    const std::shared_ptr<timer_task_t>& rhs) const
    {
        return lhs->expire() < rhs->expire();
    }
};

class evloop;

// Timer使用红黑树来管理所有定时事件，这在添加和删除上
// 都可以做到足够高效。最小堆和红黑树的存取效率应该是相差无几的，
// 至于时间轮，这里我们不需要那么复杂的Timer
//
// Timer一般有两种驱动方法：
// 一种是SIGALRM信号，但在多线程环境下我们一般避免去处理信号
// 另一种就是I/O多路复用的超时参数了，这也是我们这里所使用的
// 其实在linux下，还用第三种方法，即timerfd
class timer_t : noncopyable {
public:
    explicit timer_t(evloop *loop) : loop(loop), timer_id(1)
    {
    }
    ~timer_t() {  };

    size_t add_timer(timer_task_t *task);
    // 返回最小超时值
    // set是可以在O(1)时间获取最小节点的，这只需要维护一个指向最小
    // 节点的指针即可，因此完全可以达到与使用最小堆相同的效果。
    int64_t timeout()
    {
        int64_t timeval;
        if (!timer_set.empty()) {
            timeval = timer_set.begin()->get()->expire() - util::get_cur_time_ms();
            timeval = timeval > 0 ? timeval : 0;
        } else
            timeval = -1;
        return timeval;
    }
    void cancel_timer(size_t id);
    // 处理所有到期的定时事件
    void tick();
private:
    void add_timer_in_loop(timer_task_t *task, size_t id);
    void cancel_timer_in_loop(size_t id);

    // 这里之所以要用到TimerIterator，是因为multiset提供两个erase()，
    // 一个以值为参数，一个以迭代器为参数，如果存有重复的元素，则前者
    // 会删掉所有值相同的元素，而后者只会删掉迭代器指向的那个元素，
    // 很显然我们想要的是后者
    typedef std::multiset<std::shared_ptr<timer_task_t>>::iterator timer_iterator;
    void del_timer(const timer_iterator it)
    { timer_set.erase(it); timer_map.erase(it->get()->id()); }
    // 删除最小定时器
    void del_timer() { del_timer(timer_set.begin()); }

    void update_timer(int64_t now);

    evloop *loop;
    // TimerTask中的expire可能会有重复
    std::multiset<std::shared_ptr<timer_task_t>, timer_task_cmp> timer_set;
    std::unordered_map<size_t, std::shared_ptr<timer_task_t>> timer_map;
    // 唯一标识一个定时器，是全局递增的
    size_t timer_id;
};
}

#endif // _ANGEL_TIMERTASK_H
