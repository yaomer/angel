#ifndef _ANGEL_CONNECTION_H
#define _ANGEL_CONNECTION_H

#include <functional>
#include <memory>
#include <atomic>
#include <any> // only c++17
#include <queue>

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
// Called when the data to be sent accumulates to a certain threshold.
typedef std::function<void(const connection_ptr&)> high_water_mark_handler_t;
// Called after all previous data has been sent.
typedef std::function<void(const connection_ptr&)> send_complete_handler_t;

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
    virtual ~connection();

    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;

    size_t id() const { return conn_id; }
    evloop *get_loop() { return loop; }
    inet_addr& get_local_addr() { return local_addr; }
    inet_addr& get_peer_addr() { return peer_addr; }
    bool is_connected() { return conn_state == state::connected; }
    bool is_closing() { return conn_state == state::closing; }
    bool is_closed() { return conn_state == state::closed; }
    // Save the context needed for the connection. (used by user)
    //
    // There should not be a connection_ptr in the context,
    // otherwise it will cause a circular reference.
    //
    // If necessary,
    // <connection*> or <weak_ptr<connection>> can be.
    void set_context(std::any ctx) { context = std::move(ctx); }
    std::any& get_context() { return context; }
    // Set idle TTL(Time to Live) for connection.
    void set_ttl(int64_t ms);
    // Close connection, there are two kinds of states:
    // closed: Close the connection immediately.
    // closing: Some data haven't been sent yet, so close after sending.
    void close() { handle_close(false); }

    // thread-safe
    void send(std::string_view s);
    void send(const char *s, size_t len);
    void send(const void *v, size_t len);
    void format_send(const char *fmt, ...);
    // send_file() async-sends the specified file by zero copy.
    void send_file(int fd, off_t offset, off_t count);
    // If you want to close fd after sending the file, you can do that.
    // send_file(), and then
    // set_send_complete_handler([]{ close(fd); })
    //
    // Or after send(), you can also do something with it.
    void set_send_complete_handler(const send_complete_handler_t handler);

    // Generally, they are only invoked by server and client,
    // and can not be called directly, unless you want to override
    // the handler set by server or client.
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
private:
    void handle_read();
    void handle_write();
    void handle_close(bool is_forced);
    void handle_error();
    void force_close_connection() { handle_close(true); }
    void send_in_loop(const char *data, size_t len);
    void send_file_in_loop(int fd, off_t offset, off_t count);
    void set_ttl_timer();
    void update_ttl_timer();
    const char *get_state_str();

    virtual void handle_message();
    virtual ssize_t write(const char *data, size_t len);
    virtual ssize_t sendfile(int fd, off_t offset, off_t count);

    size_t conn_id;
    evloop *loop;
    std::shared_ptr<channel> channel;
    std::unique_ptr<socket> socket;
    buffer input_buf;
    inet_addr local_addr;
    inet_addr peer_addr;
    std::any context;
    std::atomic<state> conn_state;
    size_t ttl_timer_id;
    int64_t ttl_ms;

    struct file {
        int   fd;
        off_t offset;
        off_t count;
    };
    buffer output_buf;
    // pair<send_id, output_buf offset len>
    std::queue<std::pair<size_t, size_t>> byte_stream_queue;
    std::queue<std::pair<size_t, file>> send_file_queue;
    std::queue<std::pair<size_t, send_complete_handler_t>> send_complete_handler_queue;
    bool send_queue_is_empty() { return byte_stream_queue.empty() && send_file_queue.empty(); }
    // Increment id, assigned to each send task.
    // (send byte stream) or (send file)
    size_t send_id;
    // Id of the next send task to be processed.
    size_t next_id;

    connection_handler_t connection_handler;
    message_handler_t message_handler;
    close_handler_t close_handler;
    high_water_mark_handler_t high_water_mark_handler;
    size_t high_water_mark;

    // Called by server or client after the connection is successfully established.
    // Used to register channel into evloop.
    void establish();
    void set_state(state state) { conn_state = state; }

    friend class server;
    friend class client;
    friend class ssl_connection;
};

}

#endif // _ANGEL_CONNECTION_H
