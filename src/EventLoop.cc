#include <unistd.h>
#include <csignal>
#include <thread>
#include <mutex>
#include <memory>
#include <map>
#include <vector>
#include "EventLoop.h"
#include "Timer.h"
#include "Signaler.h"
#include "LogStream.h"
#include "Socket.h"
#include "SockOps.h"
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
#ifdef _ANGEL_HAVE_SELECT
#include "Select.h"
#endif

using namespace Angel;

thread_local EventLoop *_thisThreadLoop = nullptr;

EventLoop::EventLoop()
    : _timer(new Timer(this)),
    _quit(false),
    _nloops(0)
{
#if defined (_ANGEL_HAVE_EPOLL)
    _poller.reset(new Epoll);
#elif defined (_ANGEL_HAVE_KQUEUE)
    _poller.reset(new Kqueue);
#elif defined (_ANGEL_HAVE_POLL)
    _poller.reset(new Poll);
#elif defined (_ANGEL_HAVE_SELECT)
    _poller.reset(new Select);
#else
    logFatal("No supported I/O multiplexing");
#endif
    if (_thisThreadLoop) {
        logFatal("Only have one EventLoop in this thread");
    } else
        _thisThreadLoop = this;
    _tid = std::this_thread::get_id();
    SockOps::socketpair(_wakeFd);
    std::lock_guard<std::mutex> mlock(_SYNC_INIT_LOCK);
    if (!__signalerPtr) {
        _signaler.reset(new Signaler(this));
        __signalerPtr = _signaler.get();
    }
    logInfo("[EventLoop::ctor]");
}

EventLoop::~EventLoop()
{
    logInfo("[EventLoop::dtor]");
}

void EventLoop::addChannel(const ChannelPtr& chl)
{
    runInLoop([this, chl]{ this->addChannelInLoop(chl); });
}

void EventLoop::removeChannel(const ChannelPtr& chl)
{
    runInLoop([this, chl]{ this->removeChannelInLoop(chl); });
}

void EventLoop::addChannelInLoop(const ChannelPtr& chl)
{
    chl->enableRead();
    _poller->add(chl->fd(), chl->events());
    auto it = std::pair<int, ChannelPtr>(chl->fd(), chl);
    _channelMaps.insert(it);
}

void EventLoop::removeChannelInLoop(const ChannelPtr& chl)
{
    _poller->remove(chl->fd(), chl->events());
    _channelMaps.erase(chl->fd());
}

void EventLoop::run()
{
    wakeupInit();
    if (_signaler) _signaler->start();
    while (!_quit) {
        int64_t timeout = _timer->timeout();
        logDebug("timeout = %zd", timeout);
        int nevents = _poller->wait(this, timeout);
        if (nevents > 0) {
            logDebug("nevents = %d", nevents);
            for (auto& it : _activeChannels) {
                it->handleEvent();
            }
            _activeChannels.clear();
        } else if (nevents == 0) {
            _timer->tick();
        }
        doFunctors();
        _nloops++;
    }
}

void EventLoop::doFunctors()
{
    std::vector<Functor> tfuncs;
    if (!_functors.empty()) {
        logDebug("executed %zu functors", _functors.size());
        std::lock_guard<std::mutex> mlock(_mutex);
        tfuncs.swap(_functors);
        _functors.clear();
    }
    for (auto& func : tfuncs)
        func();
}

void EventLoop::wakeupInit()
{
    auto chl = ChannelPtr(new Channel(this));
    chl->setFd(_wakeFd[0]);
    chl->setEventReadCb([this]{ this->handleRead(); });
    addChannel(chl);
}

// 唤醒io线程
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(_wakeFd[1], &one, sizeof(one));
    if (n != sizeof(one))
        logError("write %zd bytes instead of 8");
}

void EventLoop::handleRead()
{
    uint64_t one;
    ssize_t n = read(_wakeFd[0], &one, sizeof(one));
    if (n != sizeof(one))
        logError("read %zd bytes instead of 8");
}

// 判断当前线程是否是io线程
bool EventLoop::isInLoopThread()
{
    return std::this_thread::get_id() == _tid;
}

// 在io线程执行用户回调
void EventLoop::runInLoop(const Functor _cb)
{
    if (!isInLoopThread()) {
        std::lock_guard<std::mutex> mlock(_mutex);
        _functors.push_back(std::move(_cb));
        wakeup();
    } else
        _cb();
}

void EventLoop::queueInLoop(const Functor _cb)
{
    std::lock_guard<std::mutex> mlock(_mutex);
    _functors.push_back(std::move(_cb));
    wakeup();
}

size_t EventLoop::runAfter(int64_t timeout, const TimerCallback _cb)
{
    int64_t expire = TimeStamp::now() + timeout;
    TimerTask *task = new TimerTask(expire, 0, std::move(_cb));
    size_t id = _timer->addTimer(task);
    return id;
}

size_t EventLoop::runEvery(int64_t interval, const TimerCallback _cb)
{
    int64_t expire = TimeStamp::now() + interval;
    TimerTask *task = new TimerTask(expire, interval, std::move(_cb));
    size_t id = _timer->addTimer(task);
    return id;
}

void EventLoop::cancelTimer(size_t id)
{
    _timer->cancelTimer(id);
}
