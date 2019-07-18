#include "EventLoop.h"
#include "Timer.h"
#include "LogStream.h"

using namespace Angel;

Timer::Timer(EventLoop *loop)
    : _loop(loop)
{
    logInfo("[Timer::ctor]");
}

Timer::~Timer()
{
    logInfo("[Timer::dtor]");
}

size_t Timer::addTimer(TimerTask *_task)
{
    size_t id = getId();
    _loop->runInLoop(
            [this, _task, id]{ this->addTimerInLoop(_task, id); });
    return id;
}

void Timer::cancelTimer(size_t id)
{
    _loop->runInLoop(
            [this, id]{ this->cancelTimerInLoop(id); });
}

// Add a TimerTask is O(log n)
void Timer::addTimerInLoop(TimerTask *_task, size_t id)
{
    _task->setId(id);
    _timer.insert(std::shared_ptr<TimerTask>(_task));
    logInfo("Add a timer, id = %zu", id);
}

// Cancel a TimerTask is O(nlogn)
void Timer::cancelTimerInLoop(size_t id)
{
    for (auto it = _timer.begin(); it != _timer.end(); it++) {
        if ((*it)->id() == id) {
            _timer.erase(it);
            putId(id);
            logInfo("Cancel a timer, id = %zu", id);
            break;
        }
    }
}

void Timer::tick()
{
    logDebug("timer size = %zu", _timer.size());
    int64_t now = TimeStamp::now();
    while (!_timer.empty()) {
        auto task = *_timer.begin();
        if (task->expire() <= now) {
            task->timerCb()();
            if (task->interval() > 0) {
                TimerTask *newTask = new TimerTask(now + task->interval(),
                        task->interval(), task->timerCb());
                newTask->setId(task->id());
                _timer.erase(_timer.begin());
                _timer.insert(std::shared_ptr<TimerTask>(newTask));
            } else {
                _timer.erase(_timer.begin());
                putId(task->id());
            }
        } else
            break;
    }
}
