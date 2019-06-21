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
    void addChannel(std::shared_ptr<Channel> chl);
    void removeChannel(std::shared_ptr<Channel> chl);
    void changeEvent(int fd, int events)
    {
        _poller->change(fd, events);
    }
    std::shared_ptr<Channel> searchChannel(int fd)
    {
        auto it = _channelMaps.find(fd);
        return it->second;
    }
    void fillActiveChannel(std::shared_ptr<Channel> chl)
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
    size_t runAfter(int64_t timeout, const TimerCallback _cb);
    size_t runEvery(int64_t interval, const TimerCallback _cb);
    void cancelTimer(size_t id);
    void quit() { _quit = true; wakeup(); }
private:
    void addChannelInLoop(std::shared_ptr<Channel> chl);
    void removeChannelInLoop(std::shared_ptr<Channel> chl);

    std::unique_ptr<Poller> _poller;
    std::unique_ptr<Timer> _timer;
    std::unique_ptr<Signaler> _signaler;
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
