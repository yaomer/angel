#include "Timer.h"
#include "LogStream.h"

using namespace Angel;


TimerTask::TimerTask(int64_t timeout, int64_t interval, const TimerCallback _cb)
    : _id(0),
    _timeout(timeout),
    _interval(interval),
    _timerCb(_cb)
{

}

TimerTask::~TimerTask()
{

}

Timer::Timer()
{
    logInfo("[Timer::ctor]");
}

Timer::~Timer()
{
    logInfo("[Timer::dtor]");
}

// Add a TimerTask is Log(n)
size_t Timer::addTask(TimerTask *_task)
{
    size_t id = getId();
    _task->setId(id);
    _task->setTimeout(_task->timeout() + TimeStamp::now());
    _timer.insert(std::unique_ptr<TimerTask>(_task));
    return id;
}

// Cancel a TimerTask is O(n)
void Timer::cancelTask(size_t id)
{
    for (auto& it : _timer) {
        if (it->id() == id) {
            _timer.erase(it);
            putId(id);
            break;
        }
    }
}

void Timer::tick()
{
    if (_timer.empty())
        return;

    int64_t timeout = getTask()->timeout();
    while (!_timer.empty()) {
        const TimerTask *t = getTask();
        if (t->timeout() == timeout) {
            t->timerCb()();
            if (t->interval() > 0) {
                TimerTask *_t = new TimerTask(t->interval(),
                        t->interval(), t->timerCb());
                addTask(_t);
            }
            popTask();
        } else {
            break;
        }
    }
}
