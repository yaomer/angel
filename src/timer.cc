#include "timer.h"

#include <angel/evloop.h>
#include <angel/logger.h>

namespace angel {

timer_t::timer_t(evloop *loop) : loop(loop), timer_id(1)
{
}

timer_t::~timer_t()
{
}

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

// Add a timer task is O(log n)
void timer_t::add_timer_in_loop(timer_task_t *task, size_t id)
{
    task->id = id;
    auto it = std::shared_ptr<timer_task_t>(task);
    timer_set.emplace(it);
    timer_map.emplace(id, it);
    log_debug("timer(id=%zu) has been added to the Timer", id);
}

// Cancel a timer task is O(log n)
void timer_t::cancel_timer_in_loop(size_t id)
{
    auto it = timer_map.find(id);
    if (it == timer_map.end()) return;
    auto range = timer_set.equal_range(it->second);
    timer_map.erase(it);
    for (auto it = range.first; it != range.second; ++it) {
        if ((*it)->id == id) {
            log_debug("timer(id=%d) has been canceled", id);
            (*it)->canceled = true;
            timer_set.erase(it);
            break;
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
        // Remove old task first, then add new task to ensure correct execution order.
        timer_set.erase(timer_set.begin());
        timer_map.erase(task->id);
        add_timer_in_loop(new_task, new_task->id);
    } else {
        timer_set.erase(timer_set.begin());
        timer_map.erase(task->id);
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
