#include "timer.h"

#include <angel/evloop.h>
#include <angel/logger.h>

namespace angel {

// timer_id从1开始，0保留用作他用
// 比如当timer_id为0时，我们就认为它不是一个有效的timer
timer_t::timer_t(evloop *loop) : loop(loop), timer_id(1)
{
}

timer_t::~timer_t() = default;

int64_t timer_t::timeout()
{
    int64_t timeval;
    if (!timer_set.empty()) {
        timeval = timer_set.begin()->get()->expire - util::get_cur_time_ms();
        timeval = timeval > 0 ? timeval : 0;
    } else
        timeval = -1;
    return timeval;
}

size_t timer_t::add_timer(timer_task_t *task)
{
    size_t id = timer_id++;
    loop->run_in_loop(
            [this, task, id]{ this->add_timer_in_loop(task, id); });
    return id;
}

void timer_t::cancel_timer(size_t id)
{
    loop->run_in_loop(
            [this, id]{ this->cancel_timer_in_loop(id); });
}

// Add a TimerTask is O(log n)
void timer_t::add_timer_in_loop(timer_task_t *task, size_t id)
{
    task->id = id;
    auto it = std::shared_ptr<timer_task_t>(task);
    timer_set.emplace(it);
    timer_map.emplace(id, it);
    log_debug("timer(id=%zu) has been added to the Timer", id);
}

// Cancel a TimerTask is O(logn)
void timer_t::cancel_timer_in_loop(size_t id)
{
    auto it = timer_map.find(id);
    if (it != timer_map.end()) {
        // 对于multiset来说，由于它存储的值可重复，所以它不能简单的
        // 用find()来查找，因为如果存在多个相同的元素，这无法确定
        // 到底返回哪个，而我们则需要返回所有相同的元素，有两种方法
        // 可以做到：
        // 1. 使用equal_range()，它返回一个pair，[first, second)区间
        // 表示所有匹配的元素
        // 2. [lower_bound(), upper_bound()) <==> [first, second)
        // 这两种方法实际上是大同小异的，看个人喜好了
        auto range = timer_set.equal_range(it->second);
        for (auto it = range.first; it != range.second; ++it) {
            if ((*it)->id == id) {
                log_debug("timer(id=%d) has been canceled", id);
                (*it)->canceled = true;
                del_timer(it);
                break;
            }
        }
    }
}

void timer_t::update_timer(int64_t now)
{
    auto& task = *timer_set.begin();
    if (task->interval > 0) {
        timer_task_t *new_task = new timer_task_t(now + task->interval,
                task->interval, task->timer_cb);
        new_task->id = task->id;
        // 必须先删除旧的task，再添加更新后的task，以保证正确的执行顺序
        del_timer();
        add_timer_in_loop(new_task, new_task->id);
    } else {
        del_timer();
    }
}

void timer_t::tick()
{
    int64_t now = util::get_cur_time_ms();
    while (!timer_set.empty()) {
        auto task = *timer_set.begin();
        if (task->expire > now) break;
        task->timer_cb();
        // task can be canceled in timer_cb()
        if (!task->canceled) {
            update_timer(now);
        }
    }
}

}
