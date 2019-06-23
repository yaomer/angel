#ifndef _ANGEL_EVENTLOOPTHREADPOOL_H
#define _ANGEL_EVENTLOOPTHREADPOOL_H

#include <memory>
#include "EventLoopThread.h"

namespace Angel {

class EventLoopThreadPool {
public:
    EventLoopThreadPool();
    ~EventLoopThreadPool();
    size_t threadNums() const { return _threadNums; }
    void setThreadNums(size_t threadNums);
    EventLoopThread *getNextThread();
private:
    void start();

    size_t _threadNums;
    std::vector<std::unique_ptr<EventLoopThread>> _threadPool;
    size_t _nextIndex;
};

}

#endif // _ANGEL_EVENTLOOPTHREADPOOL_H
