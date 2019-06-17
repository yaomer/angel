#ifndef _ANGEL_EVENTLOOP_H
#define _ANGEL_EVENTLOOP_H

#include <iostream>
#include <atomic>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include "Poller.h"
#include "Channel.h"
#include "Timer.h"
#include "Signaler.h"
#include "Noncopyable.h"

namespace Angel {

typedef std::function<void()> Functor;

class EventLoop : Noncopyable {
public:
    EventLoop();
    ~EventLoop();
    void addChannel(Channel *chl)
    {
        chl->enableRead();
        _poller->add(chl->fd(), chl->events());
        _channelMaps.insert(std::pair<int,
                std::shared_ptr<Channel>>(chl->fd(), chl));
    }
    void removeChannel(Channel *chl)
    {
        _poller->remove(chl->fd());
        _channelMaps.erase(chl->fd());
    }
    void changeEvent(int fd, int events)
    {
        _poller->change(fd, events);
    }
    std::shared_ptr<Channel> searchChannel(int fd)
    {
        auto it = _channelMaps.find(fd);
        return it->second;
    }
    void fillActiveChannel(std::shared_ptr<Channel>& chl)
    {
        _activeChannels.push_back(chl);
    }
    void run();
    void wakeupInit();
    void wakeup();
    void handleRead();
    bool isInLoopThread();
    void runInLoop(const Functor _cb);
    void doFunctors();
    size_t runAfter(int64_t timeout, const TimerTask::TimerCallback _cb);
    size_t runEvery(int64_t interval, const TimerTask::TimerCallback _cb);
    void cancelTimer(size_t id);
    void addSig(int signo, const Signaler::SignalerCallback _cb) 
    {
        _signaler.add(signo, _cb);
    }
    void cancelSig(int signo)
    {
        _signaler.cancel(signo);
    }
    void quit() { _quit = true; wakeup(); }
private:
    Poller *_poller;
    Timer _timer;
    Signaler _signaler;
    std::map<int, std::shared_ptr<Channel>> _channelMaps;
    std::vector<std::shared_ptr<Channel>> _activeChannels;
    std::atomic_bool _quit;
    std::vector<Functor> _functors;
    std::mutex _mutex;
    int _wakeFd[2];
    std::__thread_id _tid;
};

} // Angel

#endif // _ANGEL_EVENTLOOP_H
