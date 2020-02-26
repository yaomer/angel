#include <vector>
#include <memory>

#include "EventLoopThread.h"
#include "EventLoopThreadPool.h"
#include "LogStream.h"

using namespace Angel;

void EventLoopThreadPool::start(size_t threadNums)
{
    if (threadNums > maxthreads) {
        logWarn("A maximum of %zu io-threads can be started", maxthreads);
        return;
    }
    _threadNums = threadNums;
    logInfo("started %zu io threads", threadNums);
    for (size_t i = 0; i < _threadNums; i++) {
        // auto it = std::unique_ptr<EventLoopThread>(new EventLoopThread);
        _threadPool.emplace_back(new EventLoopThread);
    }
    // 等待所有IO线程完成初始化
    for (size_t i = 0; i < _threadNums; i++) {
        while (!_threadPool[i]->getLoop())
            ;
    }
    logInfo("EventLoopThreadPool is running");
}

// 使用[round-robin]挑选出下一个[io thread]
EventLoopThread *EventLoopThreadPool::getNextThread()
{
    if (_nextIndex >= _threadNums) _nextIndex = 0;
    return _threadPool[_nextIndex++].get();
}
