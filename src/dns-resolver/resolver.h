#ifndef __ANGEL_RESOLVER_H
#define __ANGEL_RESOLVER_H

#include <angel/evloop_thread.h>
#include <angel/client.h>

#include <vector>
#include <queue>
#include <unordered_map>
#include <future>
#include <memory>
#include <variant>

namespace angel {

namespace dns {

// <ip1, ip2, ...>
typedef std::vector<std::string> a_res_t;
typedef std::vector<std::string> ns_res_t;
// A domain name can usually only point to one authoritative domain name
typedef std::string cname_res_t;
// <<preference1, exchange_name1>, <preference2, exchange_name2>, ...>
typedef std::vector<std::pair<uint16_t, std::string>> mx_res_t;
typedef std::vector<std::string> txt_res_t;

typedef std::variant<std::string,
                     std::vector<std::string>,
                     std::vector<std::pair<uint16_t, std::string>> > result;
typedef std::shared_future<result> result_future;

static inline const a_res_t& get_a(const result& res)
{
    return std::get<a_res_t>(res);
}

static inline const ns_res_t& get_ns(const result& res)
{
    return std::get<ns_res_t>(res);
}

static inline const cname_res_t& get_cname(const result& res)
{
    return std::get<cname_res_t>(res);
}

static inline const mx_res_t& get_mx(const result& res)
{
    return std::get<mx_res_t>(res);
}

static inline const txt_res_t& get_txt(const result& res)
{
    return std::get<txt_res_t>(res);
}

struct query_context {
    uint16_t id;
    std::string buf;
    std::promise<result> recv_promise;
    void pack(std::string_view name, uint16_t q_type, uint16_t q_class);
};

typedef std::lock_guard<std::mutex> lock_t;

class resolver {
public:
    resolver();
    result_future query(std::string_view name, std::string_view type);
private:
    static void delay_send(resolver *r, query_context *qc);
    int unpack(angel::buffer& res_buf);
    result_future query(std::string_view name, uint16_t q_type, uint16_t q_class);

    // The background thread runs an `evloop` to receive the response
    angel::evloop_thread loop_thread;
    angel::evloop *loop; // -> loop_thread.get_loop()
    std::unique_ptr<angel::client> cli;
    std::atomic_size_t id = 1;
    typedef std::unordered_map<uint16_t, std::unique_ptr<query_context>> QueryMap;
    typedef std::queue<std::packaged_task<void()>> DelayTaskQueue;
    QueryMap query_map;
    std::mutex query_map_mutex;
    DelayTaskQueue delay_task_queue;
    std::mutex delay_task_queue_mutex;
};

}
}

#endif // __ANGEL_RESOLVER_H
