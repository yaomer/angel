#include "EventLoopThread.h"
#include "EventLoop.h"
#include "LogStream.h"

using namespace Angel;

EventLoopThread::EventLoopThread()
    : _loop(nullptr),
    _start(false),
    _thread([this]{ this->threadFunc(); })
{

}

EventLoopThread::~EventLoopThread()
{
    if (_loop)
        _loop->quit();
}

EventLoop *EventLoopThread::getLoop()
{
    std::unique_lock<std::mutex> mlock(_mutex);
    // wait for _loop to be constructed
    while (_loop == nullptr)
        _condVar.wait(mlock);
    return _loop;
}

void EventLoopThread::start()
{
    std::lock_guard<std::mutex> mlock(_mutex);
    _start = true;
    // let loop to run
    _condVar.notify_one();
}

void EventLoopThread::quit()
{
    _loop->quit();
}

void EventLoopThread::threadFunc()
{
    _thread.detach();
    EventLoop loop;
    {
        std::lock_guard<std::mutex> mlock(_mutex);
        _loop = &loop;
        // let getLoop() to return
        _condVar.notify_one();
    }
    {
        std::unique_lock<std::mutex> mlock(_mutex);
        while (!_start)
            _condVar.wait(mlock);
    }
    loop.run();
}
