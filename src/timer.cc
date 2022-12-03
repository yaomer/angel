#include "timer.h"

#include <angel/evloop.h>
#include <angel/logger.h>

namespace angel {

timer_t::timer_t(evloop *loop) : loop(loop), timer_id(1)
{
}

timer_t::~timer_t()
{
    log_debug("~timer_t()");
}

int64_t timer_t::timeout()
{
    int64_t timeval;
    if (!timer_set.empty()) {
        timeval = (*timer_set.begin())->expire - util::get_cur_time_ms();
        timeval = timeval > 0 ? timeval : 0;
    } else
        timeval = -1;
    return timeval;
}

size_t timer_t::add_timer(timer_task_t *task)
{
    task->id = timer_id.fetch_add(1, std::memory_order_relaxed);
    loop->run_in_loop([this, task]{ this->add_timer_in_loop(task); });
    return task->id;
}

void timer_t::cancel_timer(size_t id)
{
    if (id == 0) return;
    loop->run_in_loop([this, id]{ this->cancel_timer_in_loop(id); });
}

// Add a timer task is O(log n)
void timer_t::add_timer_in_loop(timer_task_t *task)
{
    timer_set.emplace(task);
    timer_map.emplace(task->id, task);
}

// Cancel a timer task is O(log n)
void timer_t::cancel_timer_in_loop(size_t id)
{
    auto iter = timer_map.find(id);
    if (iter == timer_map.end()) return;
    auto range = timer_set.equal_range(iter->second.get());
    for (auto it = range.first; it != range.second; ++it) {
        if ((*it)->id == id) {
            log_debug("Cancel a timer(id=%d)", id);
            timer_set.erase(it);
            timer_map.erase(iter);
            break;
        }
    }
}

void timer_t::tick()
{
    int64_t now = util::get_cur_time_ms();
    while (!timer_set.empty()) {
        // Get the minimum timeout timer.
        auto cur = *timer_set.begin();
        if (cur->expire > now) break;

        // Current timer may be canceled in timer_cb(),
        // so we "delete" (release ownership to `guard`) it in advance.
        auto it = timer_map.find(cur->id);
        std::unique_ptr<timer_task_t> guard(it->second.release());
        timer_set.erase(timer_set.begin());
        timer_map.erase(it);

        cur->timer_cb();

        // Update interval timer.
        if (cur->interval > 0) {
            // To avoid timing error accumulation,
            // we should use `cur->expire` as new base expire time instead of `now`.
            auto expire = cur->expire + cur->interval;
            auto *task = new timer_task_t(expire, cur->interval, std::move(cur->timer_cb));
            task->id = cur->id;
            add_timer_in_loop(task);
        }
    }
}

}
