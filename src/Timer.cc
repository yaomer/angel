#include "EventLoop.h"
#include "Timer.h"
#include "LogStream.h"

using namespace Angel;

Timer::Timer(EventLoop *loop)
    : _loop(loop),
    _timerId(1)
{
    logInfo("[Timer::ctor]");
}

Timer::~Timer()
{
    logInfo("[Timer::dtor]");
}

size_t Timer::addTimer(TimerTask *task)
{
    size_t id = _timerId++;
    _loop->runInLoop(
            [this, task, id]{ this->addTimerInLoop(task, id); });
    return id;
}

void Timer::cancelTimer(size_t id)
{
    _loop->runInLoop(
            [this, id]{ this->cancelTimerInLoop(id); });
}

// Add a TimerTask is O(log n)
void Timer::addTimerInLoop(TimerTask *task, size_t id)
{
    task->setId(id);
    auto it = std::shared_ptr<TimerTask>(task);
    _timer.insert(it);
    _idMaps.insert(std::make_pair(id, it));
    logInfo("Add a timer, id = %zu", id);
}

// Cancel a TimerTask is O(logn)
void Timer::cancelTimerInLoop(size_t id)
{
    auto it = _idMaps.find(id);
    if (it != _idMaps.end()) {
        auto range = _timer.equal_range(it->second);
        for (auto it = range.first; it != range.second; it++) {
            if ((*it)->id() == id) {
                delTimer(it, id);
                logInfo("Cancel a timer, id = %zu", id);
                break;
            }
        }
    }
}

void Timer::delTimer(const TimerIterator it, size_t id)
{
    _timer.erase(it);
    _idMaps.erase(id);
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
                delTimer(_timer.begin(), newTask->id());
                addTimerInLoop(newTask, newTask->id());
            } else {
                delTimer(_timer.begin(), task->id());
            }
        } else
            break;
    }
}
