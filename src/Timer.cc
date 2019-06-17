#include "TimerTask.h"
#include "Timer.h"
#include "Logger.h"

using namespace Angel;

// Add a TimerTask is Log(n)
size_t Timer::add(TimerTask *_task)
{
    size_t id;
    if (!_freeIdList.empty()) {
        id = *_freeIdList.begin();
        _freeIdList.erase(_freeIdList.begin());
    } else {
        id = _timerId++;
    }
    _task->setId(id);
    int64_t timeout = _task->timeout() + TimeStamp::now();
    _task->setTimeout(timeout);
    _timer.insert(std::unique_ptr<TimerTask>(_task));
    return id;
}

// Cancel a TimerTask is O(n)
void Timer::cancel(size_t id)
{
    for (auto& it : _timer) {
        if (it.get()->id() == id) {
            _timer.erase(it);
            _freeIdList.insert(id);
            break;
        }
    }
}

void Timer::tick()
{
    if (_timer.empty())
        return;

    int64_t timeout = get()->timeout();
    while (!_timer.empty()) {
        const TimerTask *t = get();
        if (t->timeout() == timeout) {
            t->timerCb()();
            if (t->interval() > 0) {
                TimerTask *_t = new TimerTask(t->interval(),
                        t->interval(), t->timerCb());
                add(_t);
            }
            pop();
        } else {
            break;
        }
    }
}
