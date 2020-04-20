#ifndef _ANGEL_THREAD_POOL_H
#define _ANGEL_THREAD_POOL_H

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "noncopyable.h"
#include "decls.h"
#include "LogStream.h"

namespace Angel {

class ThreadPool : noncopyable {
public:
    ThreadPool() : _threadNums(0), _quit(false)
    {
    }
    ~ThreadPool() { quit(); };
    void start(size_t threadNums)
    {
        if (threadNums > maxthreads) {
            logWarn("A maximum of %zu task-threads can be started", maxthreads);
            return;
        }
        _threadNums = threadNums;
        for (size_t i = 0; i < _threadNums; i++) {
            std::thread thread([this]{ this->threadFunc(); });
            _workers.emplace_back(std::move(thread));
        }
    }
    void addTask(const TaskCallback cb)
    {
        if (_threadNums == 0) {
            logWarn("There are no threads available");
            return;
        }
        std::lock_guard<std::mutex> mlock(_mutex);
        _taskQueue.emplace(cb);
        _condVar.notify_one();
    }
    void quit()
    {
        _quit = true;
        _condVar.notify_all();
        for (auto& thread : _workers)
            thread.join();
    }
private:
    void threadFunc()
    {
        while (true) {
            TaskCallback cb;
            {
            std::unique_lock<std::mutex> mlock(_mutex);
            while (!_quit && _taskQueue.empty())
                _condVar.wait(mlock);
            if (_quit) {
                break;
            }
            cb = std::move(_taskQueue.front());
            _taskQueue.pop();
            }
            if (cb) cb();
        }
    }

    static const size_t maxthreads = 256;
    size_t _threadNums;
    std::vector<std::thread> _workers;
    std::queue<TaskCallback> _taskQueue;
    std::mutex _mutex;
    std::condition_variable _condVar;
    std::atomic_bool _quit;
};

}

#endif // _ANGEL_THREAD_POOL_H
