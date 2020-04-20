#ifndef _ANGEL_EVENTLOOP_THREAD_H
#define _ANGEL_EVENTLOOP_THREAD_H

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "EventLoop.h"
#include "noncopyable.h"

namespace Angel {

class EventLoop;

// One-loop-per-thread
class EventLoopThread : noncopyable {
public:
    EventLoopThread()
        : _loop(nullptr),
        _thread([this]{ this->threadFunc(); })
    {
    }
    ~EventLoopThread()
    {
        quit();
    }
    EventLoop *getLoop()
    {
        std::unique_lock<std::mutex> mlock(_mutex);
        // 等待loop初始化完成
        while (_loop == nullptr)
            _condVar.wait(mlock);
        return _loop;
    }
    EventLoop *getAssertTrueLoop() { return _loop; }
    void quit()
    {
        if (_loop) _loop->quit();
        _thread.join();
    }
private:
    void threadFunc()
    {
        EventLoop loop;
        {
        std::lock_guard<std::mutex> mlock(_mutex);
        _loop = &loop;
        _condVar.notify_one();
        }
        loop.run();
    }

    EventLoop *_loop;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _condVar;
};
}

#endif // _ANGEL_EVENTLOOP_THREAD_H
