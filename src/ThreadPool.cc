#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThreadPool.h"
#include "LogStream.h"

using namespace Angel;

ThreadPool::ThreadPool(size_t threadNums)
    : _threadNums(threadNums),
    _quit(false)
{
    if (_threadNums >= _MAX_THREADS) {
        LOG_WARN << "A maximum of " << _MAX_THREADS
                 << " threads can be created";
        _threadNums = 0;
    }
    for (int i = 0; i < _threadNums; i++) {
        // _workers.push_back(std::thread([this]{ this->threadFunc(); }));
        std::thread thread([this]{ this->threadFunc(); });
        thread.detach();
        // 转移线程的管理权
        _workers.push_back(std::move(thread));
    }
    LOG_INFO << "[ThreadPool::ctor, Started " << _threadNums
             << " threads]";
}

ThreadPool::~ThreadPool()
{
    LOG_INFO << "[ThreadPool::dtor]";
    quit();
}

void ThreadPool::addTask(const TaskCallback _cb)
{
    std::lock_guard<std::mutex> mlock(_mutex);
    _qtask.push(_cb);
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
