#ifndef _ANGEL_EVENTLOOP_THREAD_POOL_H
#define _ANGEL_EVENTLOOP_THREAD_POOL_H

#include <vector>
#include <memory>

#include "EventLoopThread.h"
#include "noncopyable.h"
#include "LogStream.h"

namespace Angel {

class EventLoopThreadPool : noncopyable {
public:
    EventLoopThreadPool() : _threadNums(0), _nextIndex(0)
    {
    }
    ~EventLoopThreadPool()
    {
        for (auto& thread : _threadPool)
            thread->quit();
    }
    size_t threadNums() const { return _threadNums; }
    void start(size_t threadNums)
    {
        if (threadNums > maxthreads) {
            logWarn("A maximum of %zu io-threads can be started", maxthreads);
            return;
        }
        _threadNums = threadNums;
        for (size_t i = 0; i < _threadNums; i++) {
            _threadPool.emplace_back(new EventLoopThread);
        }
        // 等待所有IO线程完成初始化
        for (size_t i = 0; i < _threadNums; i++) {
            while (!_threadPool[i]->getLoop())
                ;
        }
    }
    // 使用[round-robin]挑选出下一个[io thread]
    EventLoopThread *getNextThread()
    {
        if (_nextIndex >= _threadNums) _nextIndex = 0;
        return _threadPool[_nextIndex++].get();
    }
private:
    static const size_t maxthreads = 256;
    size_t _threadNums;
    std::vector<std::unique_ptr<EventLoopThread>> _threadPool;
    size_t _nextIndex;
};

}

#endif // _ANGEL_EVENTLOOP_THREAD_POOL_H
