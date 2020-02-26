#include "ThreadPool.h"
#include "LogStream.h"

using namespace Angel;

void ThreadPool::start(size_t threadNums)
{
    if (threadNums < maxthreads) {
        logWarn("A maximum of %zu task-threads can be started", maxthreads);
        return;
    }
    _threadNums = threadNums;
    for (size_t i = 0; i < _threadNums; i++) {
        std::thread thread([this]{ this->threadFunc(); });
        thread.detach();
        _workers.emplace_back(std::move(thread));
    }
    logInfo("started %zu task threads", _threadNums);
}

void ThreadPool::addTask(const TaskCallback _cb)
{
    if (_threadNums == 0) {
        logWarn("There are no threads available");
        return;
    }
    std::lock_guard<std::mutex> mlock(_mutex);
    _qtask.emplace(_cb);
    _condVar.notify_one();
}

void ThreadPool::threadFunc()
{
    while (true) {
        TaskCallback cb;
        {
        std::unique_lock<std::mutex> mlock(_mutex);
        while (!_quit && _qtask.empty())
            _condVar.wait(mlock);
        if (_quit) {
            break;
        }
        cb = std::move(_qtask.front());
        _qtask.pop();
        }
        if (cb) cb();
    }
}

void ThreadPool::quit()
{
    _quit = true;
    _condVar.notify_all();
}
