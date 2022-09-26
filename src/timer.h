#ifndef _ANGEL_TIMER_H
#define _ANGEL_TIMER_H

#include <set>
#include <unordered_map>
#include <arpa/inet.h>

#include <angel/util.h>

namespace angel {

typedef std::function<void()> timer_callback_t;

struct timer_task_t {
    timer_task_t(int64_t expire_ms, int64_t interval_ms, const timer_callback_t cb)
        : id(0),
        expire(expire_ms),
        interval(interval_ms),
        timer_cb(cb),
        canceled(false)
    {
    }
    size_t id;
    int64_t expire; // timestamp (ms)
    int64_t interval;
    timer_callback_t timer_cb;
    bool canceled;
};

struct timer_task_cmp {
    bool operator()(const std::shared_ptr<timer_task_t>& lhs,
                    const std::shared_ptr<timer_task_t>& rhs) const
    {
        return lhs->expire < rhs->expire;
    }
};

class evloop;

// There are three driving methods for timer:
// 1) SIGALRM signal (But we generally avoid handling signals in a multi-threaded environment)
// 2) I/O multiplexing (we use)
// 3) timerfd (only supported by linux)
//
// There are also three data structures that implement it:
// 1) Sorted List   (Get O(1), Add O(n))
// 2) Minimum Heap or Balanced Binary Tree  (Get O(1), Add O(log n))
// 3) Time Wheel    (Get O(1), Add O(1))
class timer_t {
public:
    explicit timer_t(evloop *loop);
    ~timer_t();
    timer_t(const timer_t&) = delete;
    timer_t& operator=(const timer_t&) = delete;

    // Returns the minimum timeout value
    //
    // Set can get the node with the earliest timeout in O(1),
    // which only needs to maintain a pointer to the node.
    int64_t timeout();
    size_t add_timer(timer_task_t *task);
    void cancel_timer(size_t id);
    // Handle all expired timer events
    void tick();
private:
    void add_timer_in_loop(timer_task_t *task, size_t id);
    void cancel_timer_in_loop(size_t id);

    void update_timer(int64_t now);

    evloop *loop;
    // The timer task needs to be stored in order by expiration time,
    // so that we can execute the scheduled task according to the expiration time.
    //
    // And because expire may be repeated, we use multiset.
    std::multiset<std::shared_ptr<timer_task_t>, timer_task_cmp> timer_set;
    // Let us find a timer task by id in O(1)
    std::unordered_map<size_t, std::shared_ptr<timer_task_t>> timer_map;
    // Global increment id (increments from 1)
    // In this way, when timer_id is 0, we consider it not a valid timer task.
    size_t timer_id;
};
}

#endif // _ANGEL_TIMERTASK_H
