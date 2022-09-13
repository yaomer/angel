#ifndef _ANGEL_CLIENT_H
#define _ANGEL_CLIENT_H

#include <string>

#include <angel/evloop.h>
#include <angel/connection.h>
#include <angel/thread_pool.h>

namespace angel {

class connector_t;

struct client_options {
    bool is_reconnect = false; // 与对端断开连接后是否尝试重连
    int64_t retry_interval_ms = 3000; // connect()调用失败后的重试间隔时间
    bool is_quit_loop = true; // 连接断开后是否loop->quit()
    std::string protocol = "tcp"; // "tcp" or "udp"
};

class client {
public:
    client(evloop *, inet_addr, client_options ops = client_options());
    ~client();
    client(const client&) = delete;
    client& operator=(const client&) = delete;

    void start();
    void restart(inet_addr);
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
    void start_task_threads(size_t thread_nums = 0,
                            enum thread_pool::policy policy = thread_pool::policy::fixed);
    void executor(const task_callback_t task);
private:
    void new_connection(int fd);
    void close_connection(const connection_ptr&);

    evloop *loop;
    client_options ops;
    std::unique_ptr<connector_t> connector;
    std::shared_ptr<connection> cli_conn;
    std::unique_ptr<thread_pool> task_thread_pool;
    connection_handler_t connection_handler;
    message_handler_t message_handler;
    write_complete_handler_t write_complete_handler;
    close_handler_t close_handler;
    high_water_mark_handler_t high_water_mark_handler;
    inet_addr peer_addr;
    size_t high_water_mark;
};

}

#endif // _ANGEL_CLIENT_H
