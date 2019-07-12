#include <vector>
#include <memory>
#include "EventLoopThread.h"
#include "EventLoopThreadPool.h"
#include "LogStream.h"

using namespace Angel;

EventLoopThreadPool::EventLoopThreadPool()
    : _threadNums(0),
    _nextIndex(0)
{
    logInfo("[EventLoopThreadPool::ctor]");
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    logInfo("[EventLoopThreadPool::dtor]");
}

void EventLoopThreadPool::setThreadNums(size_t threadNums)
{
    logInfo("[EventLoopThreadPool::started %zu threads]", threadNums);
    _threadNums = threadNums;
    start();
}

void EventLoopThreadPool::start()
{
    for (int i = 0; i < _threadNums; i++) {
        auto it = std::unique_ptr<EventLoopThread>(new EventLoopThread);
        _threadPool.push_back(std::move(it));
    }
    // 等待所有线程初始化完成
    for (int i = 0; i < _threadNums; i++) {
        while (!_threadPool[i]->getLoop())
            ;
    }
}

// 使用[round-robin]挑选出下一个[io thread]
EventLoopThread *EventLoopThreadPool::getNextThread()
{
    if (_nextIndex >= _threadNums) _nextIndex = 0;
    return _threadPool[_nextIndex++].get();
}
