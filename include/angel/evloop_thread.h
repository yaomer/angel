#ifndef _ANGEL_EVLOOP_THREAD_H
#define _ANGEL_EVLOOP_THREAD_H

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <angel/evloop.h>

namespace angel {

// One-loop-per-thread
class evloop_thread {
public:
    evloop_thread()
        : loop(nullptr),
        thread([this]{ this->thread_func(); }) {  }
    ~evloop_thread() { quit(); }

    evloop_thread(const evloop_thread&) = delete;
    evloop_thread& operator=(const evloop_thread&) = delete;

    // Wait for loop to complete initialization
    evloop *wait_loop()
    {
        std::unique_lock<std::mutex> ulock(mutex);
        condvar.wait(ulock, [this]{ return this->loop; });
        return loop;
    }
    std::thread& get_thread() { return thread; }
    evloop *get_loop() { return loop; }
    void quit()
    {
        if (loop) loop->quit();
        if (thread.joinable())
            thread.join();
    }
private:
    void thread_func()
    {
        evloop t_loop;
        {
            std::lock_guard<std::mutex> mlock(mutex);
            loop = &t_loop;
            condvar.notify_one();
        }
        loop->run();
        // If not set loop to nullptr, it will become a dangling pointer
        // after exiting from thread_func().
        //
        // If you call quit() later, it will cause a segmentation fault.
        //
        loop = nullptr;
    }

    evloop *loop;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable condvar;
};
}

#endif // _ANGEL_EVLOOP_THREAD_H
