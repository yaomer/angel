#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThreadPool.h"
#include "LogStream.h"

using namespace Angel;

void ThreadPool::setThreadNums(size_t threadNums)
{
    if (_threadNums >= _MAX_THREADS) {
        logWarn("A maximum of %d threads can be created", _MAX_THREADS);
        _threadNums = 0;
    } else if (_threadNums == 0)
        _threadNums = threadNums;
    start();
}

void ThreadPool::start()
{
    for (size_t i = 0; i < _threadNums; i++) {
        std::thread thread([this]{ this->threadFunc(); });
        thread.detach();
        _workers.push_back(std::move(thread));
    }
    logInfo("[ThreadPool::started %zu threads]", _threadNums);
}

void ThreadPool::addTask(const TaskCallback _cb)
{
    if (_threadNums == 0) {
        logWarn("There are no threads available");
        return;
    }
    std::lock_guard<std::mutex> mlock(_mutex);
    _qtask.push(_cb);
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
