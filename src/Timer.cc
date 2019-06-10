#include "TimerTask.h"
#include "Timer.h"
#include "Logger.h"

void Angel::Timer::tick()
{
    if (!_timer.empty()) {
        const TimerTask *t = get();
        t->timerCb()();
        if (t->interval() > 0) {
            TimerTask *_t = new TimerTask;
            _t->setTimeout(t->interval());
            _t->setInterval(t->interval());
            _t->setTimerCb(t->timerCb());
            add(_t);
        }
        pop();
    }
}
