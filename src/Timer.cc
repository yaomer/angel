#include "EventLoop.h"
#include "Timer.h"
#include "LogStream.h"

using namespace Angel;

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
}

// Cancel a TimerTask is O(logn)
void Timer::cancelTimerInLoop(size_t id)
{
    auto it = _idMaps.find(id);
    if (it != _idMaps.end()) {
        // 对于multiset来说，由于它存储的值可重复，所以它不能简单的
        // 用find()来查找，因为如果存在多个相同的元素，这无法确定
        // 到底返回哪个，而我们则需要返回所有相同的元素，有两种方法
        // 可以做到：
        // 1. 使用equal_range()，它返回一个pair，[first, second)区间
        // 表示所有匹配的元素
        // 2. [lower_bound(), upper_bound()) <==> [first, second)
        // 这两种方法实际上是大同小异的，看个人喜好了
        auto range = _timer.equal_range(it->second);
        for (auto it = range.first; it != range.second; it++) {
            if ((*it)->id() == id) {
                (*it)->canceled();
                delTimer(it, id);
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
    int64_t now = TimeStamp::now();
    while (!_timer.empty()) {
        auto task = *_timer.begin();
        if (task->expire() <= now) {
            if (task->isCancel()) {
                delTimer(_timer.begin(), task->id());
                continue;
            }
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
