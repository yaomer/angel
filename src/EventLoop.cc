#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <mutex>
#include "EventLoop.h"
#include "Timer.h"
#include "Logger.h"
#include "LogStream.h"
#include "Socket.h"
#include "config.h"

#ifdef _ANGEL_HAVE_KQUEUE
#include "Kqueue.h"
#endif
#ifdef _ANGEL_HAVE_EPOLL
#include "Epoll.h"
#endif
#ifdef _ANGEL_HAVE_POLL
#include "Poll.h"
#endif

using namespace Angel;

EventLoop::EventLoop()
    : _signaler(this),
    _quit(false)
{
#if _ANGEL_HAVE_EPOLL
    Epoll *poller = new Epoll;
#elif _ANGEL_HAVE_KQUEUE
    Kqueue *poller = new Kqueue;
#elif _ANGEL_HAVE_POLL
    Poll *poller = new Poll;
#else
    LOG_FATAL << "No supported I/O multiplexing";
#endif
    _poller = poller;
    _tid = std::this_thread::get_id();
    Socket::socketpair(_wakeFd);
}

EventLoop::~EventLoop()
{
    delete _poller;
}

void EventLoop::run()
{
    wakeupInit();
    _signaler.start();
    while (!_quit) {
        int nevents = _poller->wait(this, _timer.timeout());
        if (nevents > 0) {
            LOG_INFO << "there are " << nevents << " active events";
            for (auto& it : _activeChannels) {
                LOG_INFO << "fd = " << it->fd() << " revents is "
                         << eventStr[it.get()->revents()];
                it.get()->handleEvent();
            }
            _activeChannels.clear();
        } else if (nevents == 0) {
            _timer.tick();
        } else {
            if (errno != EINTR)
                LOG_ERROR << "Poller::wait(): " << strerrno();
        }
        doFunctors();
    }
}

void EventLoop::doFunctors()
{
    std::vector<Functor> funcs;
    if (!_functors.empty()) {
        LOG_INFO << "execute " << _functors.size() << " functors";
        std::lock_guard<std::mutex> mlock(_mutex);
        for (auto& it : _functors) {
            funcs.push_back(it);
            _functors.pop_back();
        }
    }
    for (auto& it : funcs)
        it();
}

void EventLoop::wakeupInit()
{
    Channel *chl = new Channel(this);
    chl->setFd(_wakeFd[0]);
    chl->setReadCb([this]{ this->handleRead(); });
    addChannel(chl);
}

// 唤醒io线程
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(_wakeFd[1], &one, sizeof(one));
    if (n != sizeof(one))
        LOG_ERROR << "write " << n << " bytes instead of 8";
}

void EventLoop::handleRead()
{
    uint64_t one;
    ssize_t n = read(_wakeFd[0], &one, sizeof(one));
    if (n != sizeof(one))
        LOG_ERROR << "read " << n << " bytes instead of 8";
}

// 判断当前线程是否是io线程
bool EventLoop::isInLoopThread()
{
    return std::this_thread::get_id() == _tid;
}

// 在io线程执行用户回调
void EventLoop::runInLoop(Functor _cb)
{
    if (!isInLoopThread()) {
        std::lock_guard<std::mutex> mlock(_mutex);
        _functors.push_back(_cb);
        wakeup();
    } else {
        _cb();
    }
}

size_t EventLoop::runAfter(int64_t timeout,
                           const TimerTask::TimerCallback _cb)
{
    TimerTask *task = new TimerTask(timeout, 0, _cb);
    size_t id = _timer.add(task);
    LOG_INFO << "added a timer after " << timeout
             << " timerId = " << id;
    return id;
}

size_t EventLoop::runEvery(int64_t interval,
                           const TimerTask::TimerCallback _cb)
{
    TimerTask *task = new TimerTask(interval, interval, _cb);
    size_t id = _timer.add(task);
    LOG_INFO << "added a timer every " << interval
             << " timerId = " << id;
    return id;
}

void EventLoop::cancelTimer(size_t id)
{
    LOG_INFO << "cancel a timer, timerId = " << id;
    _timer.cancel(id);
}
