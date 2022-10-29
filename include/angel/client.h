#ifndef _ANGEL_CLIENT_H
#define _ANGEL_CLIENT_H

#include <string>

#include <angel/evloop.h>
#include <angel/connection.h>
#include <angel/thread_pool.h>

namespace angel {

class connector_t;

struct client_options {
    bool is_reconnect = false; // Whether to try to reconnect after disconnecting from the peer
    int retry_interval_ms = 3000; // Retry interval after connect() fails
    bool is_quit_loop = false; // Whether to quit loop after the connection is closed
    std::string protocol = "tcp"; // "tcp" or "udp"
};

class client {
public:
    typedef std::function<void()> connection_timeout_handler_t;

    client(evloop *, inet_addr, client_options ops = client_options());
    virtual ~client();
    client(const client&) = delete;
    client& operator=(const client&) = delete;

    virtual void start();
    virtual void restart();
    virtual void restart(inet_addr peer_addr);
    // When using ssl_client, you must use send() instead of conn()->send().
    virtual void send(std::string_view data);

    bool is_connected();
    const connection_ptr& conn() const { return cli_conn; }
    inet_addr get_peer_addr() { return peer_addr; }
    void set_connection_handler(connection_handler_t handler);
    void set_message_handler(message_handler_t handler);
    void set_close_handler(close_handler_t handler);
    void set_connection_timeout_handler(int timeout_ms, connection_timeout_handler_t handler);
    void set_high_water_mark_handler(size_t size, high_water_mark_handler_t handler);
    // select by angel if thread_nums = 0
    void start_task_threads(size_t thread_nums = 0,
                            enum thread_pool::policy policy = thread_pool::policy::fixed);
    void executor(const task_callback_t task);
private:
    void new_connection(int fd);
    void establish();
    void close_connection();
    void close_connection(functor f);
    void add_connection_timeout_timer();
    void cancel_connection_timeout_timer();

    evloop *loop;
    client_options ops;
    inet_addr peer_addr;
    std::unique_ptr<connector_t> connector;
    std::shared_ptr<connection> cli_conn;
    std::unique_ptr<thread_pool> task_thread_pool;
    connection_handler_t connection_handler;
    message_handler_t message_handler;
    close_handler_t close_handler;
    connection_timeout_handler_t connection_timeout_handler;
    high_water_mark_handler_t high_water_mark_handler;
    size_t connection_timeout_timer_id;
    int connection_timeout; // ms
    size_t high_water_mark;
    friend class ssl_client;
};

}

#endif // _ANGEL_CLIENT_H
