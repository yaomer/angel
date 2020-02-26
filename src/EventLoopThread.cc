#include "EventLoopThread.h"
#include "EventLoop.h"
#include "LogStream.h"

using namespace Angel;

EventLoopThread::EventLoopThread()
    : _loop(nullptr),
    _thread([this]{ this->threadFunc(); })
{
    _thread.detach();
    logInfo("thread %s is running", getThreadIdStr());
}

EventLoopThread::~EventLoopThread()
{
    if (_loop) _loop->quit();
    logInfo("thread %s is ready to exit", getThreadIdStr());
}

EventLoop *EventLoopThread::getLoop()
{
    std::unique_lock<std::mutex> mlock(_mutex);
    // 等待loop初始化完成
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
    _condVar.notify_one();
    }
    logInfo("ioLoop is running");
    loop.run();
}
