#ifndef _ANGEL_THREADPOOL_H
#define _ANGEL_THREADPOOL_H

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <atomic>

namespace Angel {

class ThreadPool {
public:
    typedef std::function<void()> TaskCallback;
    explicit ThreadPool(size_t nums);
    ~ThreadPool();
    void addTask(const TaskCallback _cb);
    void quit();
private:
    void threadFunc(void);

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
