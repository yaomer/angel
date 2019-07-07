#include "EventLoopThread.h"
#include "EventLoop.h"
#include "LogStream.h"

using namespace Angel;

EventLoopThread::EventLoopThread()
    : _loop(nullptr),
    _thread([this]{ this->threadFunc(); })
{
    _thread.detach();
    LOG_INFO << "[EventLoopThread::ctor]";
}

EventLoopThread::~EventLoopThread()
{
    if (_loop) _loop->quit();
    LOG_INFO << "[EventLoopThread::dtor]";
}

EventLoop *EventLoopThread::getLoop()
{
    std::unique_lock<std::mutex> mlock(_mutex);
    // wait for _loop to be constructed
    while (_loop == nullptr)
        _condVar.wait(mlock);
    return _loop;
}

void EventLoopThread::quit()
{
    _loop->quit();
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    {
        std::lock_guard<std::mutex> mlock(_mutex);
        _loop = &loop;
        // let getLoop() to return
        _condVar.notify_one();
    }
    loop.run();
}
