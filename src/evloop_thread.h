#ifndef _ANGEL_EVLOOP_THREAD_H
#define _ANGEL_EVLOOP_THREAD_H

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "evloop.h"
#include "noncopyable.h"

namespace angel {

// One-loop-per-thread
class evloop_thread : noncopyable {
public:
    evloop_thread()
        : loop(nullptr),
        thread([this]{ this->thread_func(); })
    {
    }
    ~evloop_thread()
    {
        quit();
    }
    std::thread& get_thread()
    {
        return thread;
    }
    evloop *wait_loop()
    {
        std::unique_lock<std::mutex> ulock(mutex);
        // 等待loop初始化完成
        while (loop == nullptr)
            condvar.wait(ulock);
        return loop;
    }
    evloop *get_loop()
    {
        return loop;
    }
    void quit()
    {
        if (loop) loop->quit();
        if (thread.joinable())
            thread.join();
    }
private:
    void thread_func()
    {
        evloop _loop;
        {
            std::lock_guard<std::mutex> mlock(mutex);
            loop = &_loop;
            condvar.notify_one();
        }
        loop->run();
        // loop不置空，当从thread_func()退出后就会成为悬挂指针
        // 之后如果再调用quit()就会导致段错误
        loop = nullptr;
    }

    evloop *loop;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable condvar;
};
}

#endif // _ANGEL_EVLOOP_THREAD_H
