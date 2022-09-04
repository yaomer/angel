#ifndef _ANGEL_SERVER_H
#define _ANGEL_SERVER_H

#include <map>
#include <functional>
#include <memory>

#include "evloop.h"
#include "connection.h"
#include "thread_pool.h"
#include "logger.h"

namespace angel {

class listener_t;
class evloop_thread_pool;

// server支持以下几种运行方式：
// 1. [单线程reactor]
// 2. [单线程reactor + thread pool](set_task_thread_nums())
// 3. [多线程reactor](set_io_thread_nums())
// 4. [多线程reactor + thread pool](set_io_thread_nums(), set_task_thread_nums())
class server {
public:
    typedef std::function<void(const connection_ptr&)> for_each_functor_t;

    explicit server(evloop *, inet_addr);
    ~server();
    server(const server&) = delete;
    server& operator=(const server&) = delete;

    inet_addr& listen_addr();
    // must be call in main-thread
    connection_ptr get_connection(size_t id);
    size_t get_connection_nums() const;

    // traverse one connection by id (thread-safe)
    void for_one(size_t id, const for_each_functor_t functor);
    // traverse all connections (thread-safe)
    void for_each(const for_each_functor_t functor);

    // select by angel if thread_nums = 0
    void set_io_thread_nums(size_t thread_nums = 0);
    void set_task_thread_nums(size_t thread_nums = 0);
    void executor(const task_callback_t task);
    void set_exit_handler(const signaler_handler_t handler)
    {
        exit_handler = std::move(handler);
    }

    void start();
    void quit();

    static void daemon();

    void set_connection_ttl(int64_t ms) { ttl_ms = ms; }

    void set_connection_handler(const connection_handler_t handler)
    { connection_handler = std::move(handler); }
    void set_message_handler(const message_handler_t handler)
    { message_handler = std::move(handler); }
    void set_close_handler(const close_handler_t handler)
    { close_handler = std::move(handler); }
    void set_write_complete_handler(const write_complete_handler_t handler)
    { write_complete_handler = std::move(handler); }
    void set_high_water_mark_handler(size_t size, const high_water_mark_handler_t handler)
    {
        high_water_mark = size;
        high_water_mark_handler = std::move(handler);
    }
private:
    void new_connection(int fd);
    void remove_connection(const connection_ptr& conn);
    void set_ttl_timer_if_needed(evloop *loop, const connection_ptr& conn);
    evloop* get_next_loop();
    void clean_up();

    evloop *loop;
    std::unique_ptr<listener_t> listener;
    std::unique_ptr<evloop_thread_pool> io_thread_pool;
    std::unique_ptr<thread_pool> task_thread_pool;
    std::map<size_t, connection_ptr> connection_map;
    size_t conn_id;
    int64_t ttl_ms;
    connection_handler_t connection_handler;
    message_handler_t message_handler;
    close_handler_t close_handler;
    write_complete_handler_t write_complete_handler;
    high_water_mark_handler_t high_water_mark_handler;
    signaler_handler_t exit_handler;
    size_t high_water_mark;
    // bool is_set_cpu_affinity;
};

}

#endif // _ANGEL_SERVER_H
