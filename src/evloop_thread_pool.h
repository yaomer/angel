#ifndef _ANGEL_EVLOOP_THREAD_POOL_H
#define _ANGEL_EVLOOP_THREAD_POOL_H

#include <vector>
#include <memory>

#include "evloop_thread.h"
#include "util.h"

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
        // 等待所有IO线程完成初始化
        for (size_t i = 0; i < thread_nums; i++) {
            while (!thread_pool[i]->wait_loop())
                ;
        }
    }
    ~evloop_thread_pool() = default;
    evloop_thread_pool(const evloop_thread_pool&) = delete;
    evloop_thread_pool& operator=(const evloop_thread_pool&) = delete;
    size_t threads() const { return thread_nums; }
    // 使用[round-robin]挑选出下一个[io thread]
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

#endif // _ANGEL_EVLOOP_THREAD_POOL_H
