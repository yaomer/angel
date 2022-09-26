#ifndef _ANGEL_CONNECTION_H
#define _ANGEL_CONNECTION_H

#include <functional>
#include <memory>
#include <atomic>
#include <any> // only c++17

#include <angel/channel.h>
#include <angel/buffer.h>
#include <angel/inet_addr.h>
#include <angel/socket.h>

namespace angel {

class evloop;
class connection;

typedef std::shared_ptr<connection> connection_ptr;

// Called after the connection is successfully established.
typedef std::function<void(const connection_ptr&)> connection_handler_t;
// Called after receiving the peer message.
typedef std::function<void(const connection_ptr&, buffer&)> message_handler_t;
// Called after the connection is closed.
typedef std::function<void(const connection_ptr&)> close_handler_t;
// Low water mark callback
// Called after data is sent, can be used to continuously send data.
typedef std::function<void(const connection_ptr&)> write_complete_handler_t;
// High water mark callback
// Called when the data to be sent accumulates to a certain threshold.
typedef std::function<void(const connection_ptr&)> high_water_mark_handler_t;
// Called when there is a connection error. (not used yet)
typedef std::function<void()> error_handler_t;

//
// A higher-level encapsulation than channel,
// and manage TCP (or UDP) connections exclusively.
//
class connection : public std::enable_shared_from_this<connection> {
public:
    enum class state {
        connecting,
        connected,
        closing,
        closed,
    };

    connection(size_t id, evloop *loop, int sockfd);
    ~connection();
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;

    size_t id() const { return conn_id; }
    void send(std::string_view s);
    void send(const char *s, size_t len);
    void send(const void *v, size_t len);
    void format_send(const char *fmt, ...);
    int send_file(int fd);
    evloop *get_loop() { return loop; }
    std::shared_ptr<channel>& get_channel() { return conn_channel; }
    inet_addr& get_local_addr() { return local_addr; }
    inet_addr& get_peer_addr() { return peer_addr; }
    void set_state(state state) { conn_state = state; }
    bool is_connected() { return conn_state == state::connected; }
    bool is_closed() { return conn_state == state::closed; }
    std::any& get_context() { return context; }
    void set_context(std::any ctx) { context = std::move(ctx); }
    void set_ttl(size_t timer_id, int64_t ms);
    void establish();
    void close() { handle_close(false); }
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
private:
    void handle_read();
    void handle_write();
    void handle_close(bool is_forced);
    void handle_error();
    void force_close_connection() { handle_close(true); }
    void send_in_loop(const char *data, size_t len);
    void update_ttl_timer_if_needed();
    const char *get_state_str();

    size_t conn_id;
    evloop *loop;
    std::shared_ptr<channel> conn_channel;
    std::unique_ptr<socket> conn_socket;
    buffer input_buf;
    buffer output_buf;
    inet_addr local_addr;
    inet_addr peer_addr;
    // Save the context needed for the connection
    //
    // There should not be a connection_ptr in the context,
    // otherwise it will cause a circular reference.
    //
    // If necessary, <connection*> or <weak_ptr<connection>> can be.
    //
    std::any context;
    std::atomic<state> conn_state;
    size_t ttl_timer_id;
    int64_t ttl_ms;
    connection_handler_t connection_handler;
    message_handler_t message_handler;
    write_complete_handler_t write_complete_handler;
    close_handler_t close_handler;
    error_handler_t error_handler;
    high_water_mark_handler_t high_water_mark_handler;
    size_t high_water_mark;
};

}

#endif // _ANGEL_CONNECTION_H
