#ifndef __RESOLVER_H
#define __RESOLVER_H

#include <angel/evloop_thread.h>
#include <angel/client.h>

#include <vector>
#include <queue>
#include <unordered_map>
#include <future>

typedef std::vector<std::string> result;

struct query_context {
    uint16_t id;
    std::string buf;
    std::promise<result> recv_promise;
    void pack(std::string name, uint16_t q_type, uint16_t q_class);
};

typedef std::lock_guard<std::mutex> lock_t;

class resolver {
public:
    resolver();
    std::shared_future<result> query(const std::string& name, const std::string& type);
private:
    static void delay_send(resolver *r, query_context *qc);
    int unpack(angel::buffer& res_buf);
    std::shared_future<result> query(const std::string& name, uint16_t q_type, uint16_t q_class);

    angel::evloop_thread loop_thread;
    std::unique_ptr<angel::client> cli;
    std::atomic_size_t id = 1;
    typedef std::unordered_map<uint16_t, std::unique_ptr<query_context>> QueryMap;
    typedef std::queue<std::packaged_task<void()>> DelayTaskQueue;
    QueryMap query_map;
    std::mutex query_map_mutex;
    DelayTaskQueue delay_task_queue;
    std::mutex delay_task_queue_mutex;
};

#endif // __RESOLVER_H
