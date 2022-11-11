#ifndef __ANGEL_EVLOOP_THREAD_POOL_H
#define __ANGEL_EVLOOP_THREAD_POOL_H

#include <vector>
#include <memory>

#include <angel/evloop_thread.h>
#include <angel/util.h>

namespace angel {

class evloop_thread_pool {
public:
    explicit evloop_thread_pool(
            size_t nums = std::thread::hardware_concurrency(),
            bool is_set_cpu_affinity = false)
        : thread_nums(nums), next_index(0)
    {
        for (size_t i = 0; i < thread_nums; i++) {
            thread_pool.emplace_back(new evloop_thread);
        }
        if (is_set_cpu_affinity) {
            int cpu_number = 0;
            int cpus = util::get_ncpus();
            for (auto& evthr : thread_pool) {
                if (cpu_number == cpus) cpu_number = 0;
                util::set_thread_affinity(
                        evthr->get_thread().native_handle(), cpu_number++);
            }
        }
    }
    ~evloop_thread_pool() = default;
    evloop_thread_pool(const evloop_thread_pool&) = delete;
    evloop_thread_pool& operator=(const evloop_thread_pool&) = delete;
    size_t threads() const { return thread_nums; }
    // Select an io loop by using [round-robin]
    evloop *get_next_loop()
    {
        if (next_index >= thread_nums) next_index = 0;
        return thread_pool[next_index++]->get_loop();
    }
private:
    size_t thread_nums;
    std::vector<std::unique_ptr<evloop_thread>> thread_pool;
    size_t next_index;
};

}

#endif // __ANGEL_EVLOOP_THREAD_POOL_H
