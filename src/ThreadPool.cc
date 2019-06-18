#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThreadPool.h"

using namespace Angel;

ThreadPool::ThreadPool(size_t nums)
    : _threadNums(nums),
    _quit(false)
{
    for (int i = 0; i < _threadNums; i++) {
        // _workers.push_back(std::thread([this]{ this->threadFunc(); }));
        std::thread thread([this]{ this->threadFunc(); });
        thread.detach();
        // 转移线程的管理权
        _workers.push_back(std::move(thread));
    }
}

ThreadPool::~ThreadPool()
{
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
        std::unique_lock<std::mutex> mlock(_mutex);
        while (!_quit && _qtask.empty())
            _condVar.wait(mlock);
        if (_quit) {
            break;
        }
        // cb()必须在临界区外执行
        TaskCallback cb = std::move(_qtask.front());
        _qtask.pop();
        mlock.unlock();
        cb();
    }
}

void ThreadPool::quit()
{
    _quit = true;
    _condVar.notify_all();
    // for (int i = 0; i < _threadNums; i++) {
        // _workers[i].join();
    // }
}
