#ifndef __ANGEL_EVLOOP_THREAD_H
#define __ANGEL_EVLOOP_THREAD_H

#include <functional>
#include <thread>
#include <future>

#include <angel/evloop.h>

namespace angel {

// One-loop-per-thread
class evloop_thread {
public:
    evloop_thread() : loop(nullptr)
    {
        auto f = barrier.get_future();
        std::thread t(&evloop_thread::thread_func, this);
        loop_thread.swap(t);
        // Wait for loop to complete initialization
        f.wait();
    }
    ~evloop_thread() { join(); }

    evloop_thread(const evloop_thread&) = delete;
    evloop_thread& operator=(const evloop_thread&) = delete;

    std::thread& get_thread() { return loop_thread; }
    evloop *get_loop() { return loop; }
    void join()
    {
        if (loop) loop->quit();
        if (loop_thread.joinable())
            loop_thread.join();
    }
private:
    void thread_func()
    {
        evloop t_loop;
        loop = &t_loop;
        barrier.set_value();
        loop->run();
        // If not set loop to nullptr, it will become a dangling pointer
        // after exiting from thread_func().
        //
        // If you call join() later, it will cause a segmentation fault.
        //
        loop = nullptr;
    }

    evloop *loop;
    std::thread loop_thread;
    std::promise<void> barrier;
};
}

#endif // __ANGEL_EVLOOP_THREAD_H
