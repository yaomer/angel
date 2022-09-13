#ifndef _ANGEL_THREAD_POOL_H
#define _ANGEL_THREAD_POOL_H

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace angel {

typedef std::function<void()> task_callback_t;

class thread_pool {
public:
    enum class policy { fixed, cached };
    enum class state { running, shutdown, stop };
    thread_pool(policy policy, unsigned nums = std::thread::hardware_concurrency())
        : policy(policy), state(state::running)
    {
        for (unsigned i = 0; i < nums; i++)
            add_new_worker();
    }
    ~thread_pool()
    {
        shutdown();
    }

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;

    void executor(const task_callback_t task)
    {
        assert(state == state::running);
        assert(!workers.empty());
        std::lock_guard<std::mutex> mlock(mutex);
        if (policy == policy::cached) {
            if (qtask.size() == workers.size()) {
                add_new_worker();
            }
        }
        qtask.emplace(std::move(task));
        condvar.notify_one();
    }
    void shutdown()
    {
        notify(state::shutdown);
        join();
    }
    void stop()
    {
        notify(state::stop);
        join();
    }
private:
    void thread_func()
    {
        while (true) {
            task_callback_t task;
            {
                std::unique_lock<std::mutex> ulock(mutex);
                while (state == state::running && qtask.empty())
                    condvar.wait(ulock);
                if (state == state::stop)
                    break;
                if (!qtask.empty()) {
                    task = std::move(qtask.front());
                    qtask.pop();
                }
            }
            if (task) task();
            std::lock_guard<std::mutex> mlock(mutex);
            if (state == state::shutdown && qtask.empty())
                break;
        }
    }
    void add_new_worker()
    {
        std::thread new_thread([this]{ this->thread_func(); });
        workers.emplace_back(std::move(new_thread));
    }
    void notify(state s)
    {
        std::lock_guard<std::mutex> mlock(mutex);
        state = s;
        condvar.notify_all();
    }
    void join()
    {
        for (auto& it : workers) {
            if (it.joinable())
                it.join();
        }
    }

    std::mutex mutex;
    std::condition_variable condvar;
    std::queue<task_callback_t> qtask;
    std::vector<std::thread> workers;
    policy policy;
    state state;
};
}

#endif // _ANGEL_THREAD_POOL_H
