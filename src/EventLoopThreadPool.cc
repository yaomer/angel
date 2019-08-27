#include <vector>
#include <memory>
#include "EventLoopThread.h"
#include "EventLoopThreadPool.h"
#include "LogStream.h"

using namespace Angel;

void EventLoopThreadPool::start(size_t threadNums)
{
    _threadNums = threadNums;
    logInfo("started %zu io threads", threadNums);
    for (size_t i = 0; i < _threadNums; i++) {
        // auto it = std::unique_ptr<EventLoopThread>(new EventLoopThread);
        _threadPool.emplace_back(new EventLoopThread);
    }
    logInfo("wait for all threads to complete-init");
    for (size_t i = 0; i < _threadNums; i++) {
        while (!_threadPool[i]->getLoop())
            ;
    }
    logInfo("EventLoopThreadPool is started");
}

// 使用[round-robin]挑选出下一个[io thread]
EventLoopThread *EventLoopThreadPool::getNextThread()
{
    if (_nextIndex >= _threadNums) _nextIndex = 0;
    return _threadPool[_nextIndex++].get();
}
