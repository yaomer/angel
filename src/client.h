#ifndef _ANGEL_CLIENT_H
#define _ANGEL_CLIENT_H

#include <string>

#include "evloop.h"
#include "connector.h"
#include "connection.h"
#include "thread_pool.h"
#include "noncopyable.h"
#include "enum_bitwise.h"

namespace angel {

class client : noncopyable {
public:
    enum class flags {
        exit_loop   = 01,
        disconnect  = 02,
    };
    client(evloop *, inet_addr, int64_t reconnect_interval_ms = 3000);
    ~client();
    void start();
    const connection_ptr& conn() const { return cli_conn; }
    void not_exit_loop() { flag &= ~flags::exit_loop; };
    bool is_connected() { return connector->is_connected() && cli_conn; }
    // void set_reconnect_time(int64_t ms) {  }
    void set_connection_handler(const connection_handler_t handler)
    { connection_handler = std::move(handler); }
    void set_message_handler(const message_handler_t handler)
    { message_handler = std::move(handler); }
    void set_write_complete_handler(const write_complete_handler_t handler)
    { write_complete_handler = std::move(handler); }
    void set_close_handler(const close_handler_t handler)
    { close_handler = std::move(handler); }
    void set_high_water_mark_handler(size_t size, const high_water_mark_handler_t handler)
    {
        high_water_mark = size;
        high_water_mark_handler = std::move(handler);
    }
    void set_task_thread_nums(size_t thread_nums = 0)
    {
        if (thread_nums > 0)
            task_thread_pool.reset(
                    new thread_pool(thread_pool::policy::fixed, thread_nums));
        else
            task_thread_pool.reset(
                    new thread_pool(thread_pool::policy::fixed));

    }
    void executor(const task_callback_t task)
    {
        if (!task_thread_pool) {
            log_debug("task_thread_pool is null");
            return;
        }
        task_thread_pool->executor(task);
    }
private:
    void new_connection(int fd);
    void close_connection(const connection_ptr&);

    evloop *loop;
    std::unique_ptr<connector_t> connector;
    std::shared_ptr<connection> cli_conn;
    std::unique_ptr<thread_pool> task_thread_pool;
    flags flag;
    connection_handler_t connection_handler;
    message_handler_t message_handler;
    write_complete_handler_t write_complete_handler;
    close_handler_t close_handler;
    high_water_mark_handler_t high_water_mark_handler;
    size_t high_water_mark;
};

}

#endif // _ANGEL_CLIENT_H
