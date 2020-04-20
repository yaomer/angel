#ifndef _ANGEL_EVENTLOOP_THREAD_POOL_H
#define _ANGEL_EVENTLOOP_THREAD_POOL_H

#include <memory>

#include "EventLoopThread.h"
#include "noncopyable.h"

namespace Angel {

class EventLoopThreadPool : noncopyable {
public:
    EventLoopThreadPool() : _threadNums(0), _nextIndex(0)
    {
    }
    ~EventLoopThreadPool();
    size_t threadNums() const { return _threadNums; }
    void start(size_t threadNums);
    EventLoopThread *getNextThread();
private:
    static const size_t maxthreads = 256;
    size_t _threadNums;
    std::vector<std::unique_ptr<EventLoopThread>> _threadPool;
    size_t _nextIndex;
};

}

#endif // _ANGEL_EVENTLOOP_THREAD_POOL_H
