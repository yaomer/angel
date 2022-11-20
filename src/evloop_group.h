#ifndef __ANGEL_EVLOOP_GROUP_H
#define __ANGEL_EVLOOP_GROUP_H

#include <vector>
#include <memory>

#include <angel/evloop_thread.h>
#include <angel/util.h>

namespace angel {

class evloop_group {
public:
    evloop_group(
            size_t nums = std::thread::hardware_concurrency(),
            bool is_set_cpu_affinity = false)
        : next_index(0)
    {
        for (size_t i = 0; i < nums; i++) {
            group.emplace_back(new evloop_thread);
        }
        if (is_set_cpu_affinity) {
            int cpu_number = 0;
            int cpus = util::get_ncpus();
            for (auto& evthr : group) {
                if (cpu_number == cpus) cpu_number = 0;
                util::set_thread_affinity(
                        evthr->get_thread().native_handle(), cpu_number++);
            }
        }
    }
    ~evloop_group() = default;

    evloop_group(const evloop_group&) = delete;
    evloop_group& operator=(const evloop_group&) = delete;

    size_t size() const { return group.size(); }
    // Select an io loop by using [round-robin]
    evloop *get_next_loop()
    {
        if (next_index >= group.size()) next_index = 0;
        return group[next_index++]->get_loop();
    }
private:
    size_t next_index;
    std::vector<std::unique_ptr<evloop_thread>> group;
};

}

#endif // __ANGEL_EVLOOP_GROUP_H
