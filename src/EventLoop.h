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
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class EventLoop : noncopyable {
public:
    EventLoop();
    ~EventLoop();
    void addChannel(const ChannelPtr& chl);
    void removeChannel(const ChannelPtr& chl);

    void changeEvent(int fd, int events)
    { _poller->change(fd, events); }

    ChannelPtr searchChannel(int fd)
    { return _channelMaps.find(fd)->second; }

    void fillActiveChannel(ChannelPtr& chl)
    { _activeChannels.push_back(chl); }

    void run();
    void wakeup();
    bool isInLoopThread();
    void runInLoop(const Functor _cb);
    size_t runAfter(int64_t timeout, const TimerCallback _cb);
    size_t runEvery(int64_t interval, const TimerCallback _cb);
    void cancelTimer(size_t id);
    void quit() { _quit = true; wakeup(); }
private:
    void wakeupInit();
    void handleRead();
    void addChannelInLoop(const ChannelPtr& chl);
    void removeChannelInLoop(const ChannelPtr& chl);
    void doFunctors();

    std::unique_ptr<Poller> _poller;
    std::unique_ptr<Timer> _timer;
    std::unique_ptr<Signaler> _signaler;
    // typedef std::shared_ptr<Channel> ChannelPtr
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
