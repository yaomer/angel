#ifndef _ANGEL_CLIENT_H
#define _ANGEL_CLIENT_H

#include <string>

#include "evloop.h"
#include "connection.h"
#include "thread_pool.h"

namespace angel {

class connector_t;

class client {
public:
    // is_reconnect: 与对端断开连接后是否尝试重连
    // retry_interval_ms: connect()调用失败后的重试间隔时间
    client(evloop *, inet_addr, bool is_reconnect = false, int64_t retry_interval_ms = 3000);
    ~client();
    client(const client&) = delete;
    client& operator=(const client&) = delete;

    void start();
    void restart(inet_addr);
    void not_exit_loop();
    bool is_connected();
    const connection_ptr& conn() const { return cli_conn; }
    inet_addr get_peer_addr() { return peer_addr; }
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
    // select by angel if thread_nums = 0
    void set_task_thread_nums(size_t thread_nums = 0);
    void executor(const task_callback_t task);
private:
    void new_connection(int fd);
    void close_connection(const connection_ptr&);

    evloop *loop;
    std::unique_ptr<connector_t> connector;
    std::shared_ptr<connection> cli_conn;
    std::unique_ptr<thread_pool> task_thread_pool;
    connection_handler_t connection_handler;
    message_handler_t message_handler;
    write_complete_handler_t write_complete_handler;
    close_handler_t close_handler;
    high_water_mark_handler_t high_water_mark_handler;
    inet_addr peer_addr;
    bool is_reconnect;
    bool is_exit_loop;
    int64_t retry_interval_ms;
    size_t high_water_mark;
};

}

#endif // _ANGEL_CLIENT_H
