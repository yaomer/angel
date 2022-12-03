#ifndef __ANGEL_CLIENT_H
#define __ANGEL_CLIENT_H

#include <string>
#include <atomic>

#include <angel/evloop.h>
#include <angel/connection.h>
#include <angel/thread_pool.h>

namespace angel {

class connector_t;

struct client_options {
    const char *protocol = "tcp"; // "tcp" or "udp"
    bool keep_reconnect = false;  // Ignore connect(2) error and keep trying to reconnect.
    int retry_interval_ms = 3000; // Retry interval after connect(2) fails. (for keep_reconnect)
    bool is_reconnect = false;    // Whether to try to reconnect after disconnecting from the peer.
    bool is_quit_loop = false;    // Whether to quit loop after the connection is closed.
};

class client {
public:
    typedef std::function<void()> connection_failure_handler_t;
    typedef std::function<void()> connection_timeout_handler_t;

    client(evloop *, inet_addr, client_options ops = client_options());
    virtual ~client();

    client(const client&) = delete;
    client& operator=(const client&) = delete;

    void start();
    void restart();
    void restart(inet_addr peer_addr);

    bool is_connected() const;
    const connection_ptr& conn() const;
    const inet_addr& get_peer_addr() const { return peer_addr; }

    void set_connection_handler(connection_handler_t handler);
    void set_connection_failure_handler(connection_failure_handler_t handler);
    void set_connection_timeout_handler(int timeout_ms, connection_timeout_handler_t handler);
    void set_high_water_mark_handler(size_t size, high_water_mark_handler_t handler);
    void set_message_handler(message_handler_t handler);
    void set_close_handler(close_handler_t handler);
    // select by angel if thread_nums = 0
    void start_task_threads(size_t thread_nums = 0,
                            enum thread_pool::policy policy = thread_pool::policy::fixed);
    void executor(const task_callback_t task);
private:
    virtual connection_ptr create_connection(channel *chl);
    virtual void establish(channel *chl);
    void shutdown(const connection_ptr&);
    void active_shutdown();
    void add_connection_timeout_timer();
    void cancel_connection_timeout_timer();

    static size_t get_next_id();

    evloop *loop;
    client_options ops;
    inet_addr peer_addr;
    std::unique_ptr<connector_t> connector;
    std::atomic_bool connected;
    connection_ptr cli_conn;
    std::unique_ptr<thread_pool> task_thread_pool;
    connection_handler_t connection_handler;
    connection_failure_handler_t connection_failure_handler;
    connection_timeout_handler_t connection_timeout_handler;
    high_water_mark_handler_t high_water_mark_handler;
    message_handler_t message_handler;
    close_handler_t close_handler;
    size_t connection_timeout_timer_id;
    int connection_timeout; // ms
    size_t high_water_mark;
    friend class ssl_client;
};

}

#endif // __ANGEL_CLIENT_H
