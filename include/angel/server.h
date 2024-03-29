#ifndef __ANGEL_SERVER_H
#define __ANGEL_SERVER_H

#include <unordered_map>
#include <functional>
#include <memory>

#include <angel/evloop.h>
#include <angel/connection.h>
#include <angel/thread_pool.h>
#include <angel/logger.h>

namespace angel {

class listener_t;
class evloop_group;

// Server supports several operating modes:
// 1) [single-threaded reactor]
// 2) [single-threaded reactor + thread pool] (start_task_threads())
// 3) [multi-threaded reactor] (start_io_threads())
// 4) [multi-threaded reactor + thread pool] (start_io_threads(), start_task_threads())
class server {
public:
    typedef std::function<void(const connection_ptr&)> for_each_functor_t;

    explicit server(evloop *, inet_addr);
    ~server();

    server(const server&) = delete;
    server& operator=(const server&) = delete;

    const inet_addr& listen_addr() const;
    // must be call in main-thread
    connection_ptr get_connection(size_t id);
    size_t get_connection_nums() const;

    // traverse one connection by id (thread-safe)
    void for_one(size_t id, const for_each_functor_t functor);
    // traverse all connections (thread-safe)
    void for_each(const for_each_functor_t functor);

    // select by angel if thread_nums = 0
    void start_io_threads(size_t thread_nums = 0);
    void start_task_threads(size_t thread_nums = 0,
                            enum thread_pool::policy policy = thread_pool::policy::fixed);
    // execute a task in the task thread pool
    void executor(const task_callback_t task);

    void set_nodelay(bool on);
    void set_keepalive(bool on);
    void set_keepalive_idle(int idle);
    void set_keepalive_intvl(int intvl);
    void set_keepalive_probes(int probes);

    void set_connection_handler(const connection_handler_t handler)
    { connection_handler = std::move(handler); }
    void set_message_handler(const message_handler_t handler)
    { message_handler = std::move(handler); }
    void set_close_handler(const close_handler_t handler)
    { close_handler = std::move(handler); }
    void set_high_water_mark_handler(size_t size, const high_water_mark_handler_t handler)
    {
        high_water_mark = size;
        high_water_mark_handler = std::move(handler);
    }

    void start();
    void quit();

    static void daemon();
private:
    channel *make_channel(int fd);
    virtual connection_ptr create_connection(channel *);
    virtual void establish(channel *);
    void remove_connection(const connection_ptr& conn);
    evloop* get_next_loop();
    void handle_signals();
    void clean_up();

    evloop *loop;
    std::unique_ptr<listener_t> listener;
    std::unique_ptr<evloop_group> io_loop_group;
    std::unique_ptr<thread_pool> task_thread_pool;
    std::unordered_map<size_t, connection_ptr> connection_map;
    size_t conn_id;
    connection_handler_t connection_handler;
    message_handler_t message_handler;
    close_handler_t close_handler;
    high_water_mark_handler_t high_water_mark_handler;
    size_t high_water_mark;
    // bool is_set_cpu_affinity;
    friend class ssl_server;
};

}

#endif // __ANGEL_SERVER_H
