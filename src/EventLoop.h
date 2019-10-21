#ifndef _ANGEL_EVENTLOOP_H
#define _ANGEL_EVENTLOOP_H

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

// reactor模式的核心，事件循环的驱动
class EventLoop : noncopyable {
public:
    EventLoop();
    // 向loop中注册一个事件
    void addChannel(const ChannelPtr& chl);
    // 从loop中删除一个事件
    void removeChannel(const ChannelPtr& chl);

    // 修改关联到fd上的事件
    void changeEvent(int fd, int events)
    { _poller->change(fd, events); }

    ChannelPtr searchChannel(int fd)
    { return _channelMaps.find(fd)->second; }

    void fillActiveChannel(ChannelPtr& chl)
    { _activeChannels.emplace_back(chl); }

    void run();
    void wakeup();
    bool isInLoopThread();
    void runInLoop(const Functor _cb);
    void queueInLoop(const Functor _cb);
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
    // 一个任务队列，用于将非io线程的任务转到io线程执行
    std::vector<Functor> _functors;
    std::mutex _mutex;
    int _wakeFd[2];
    std::thread::id _tid;
    size_t _nloops;
};

} // Angel

#endif // _ANGEL_EVENTLOOP_H
