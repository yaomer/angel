#ifndef _ANGEL_THREADPOOL_H
#define _ANGEL_THREADPOOL_H

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class ThreadPool : noncopyable {
public:
    ThreadPool()
        : _threadNums(0),
        _quit(false)
    {
    }
    explicit ThreadPool(size_t threadNums)
        : _threadNums(threadNums),
        _quit(false)
    {
        setThreadNums(_threadNums);
    }
    ~ThreadPool() { quit(); };
    void setThreadNums(size_t threadNums);
    void addTask(const TaskCallback _cb);
    void quit();
private:
    void start();
    void threadFunc();

    static const size_t _MAX_THREADS = 256;

    size_t _threadNums;
    std::vector<std::thread> _workers;
    std::queue<TaskCallback> _qtask;
    std::mutex _mutex;
    std::condition_variable _condVar;
    std::atomic_bool _quit;
};
}

#endif // _ANGEL_THREADPOOL_H
