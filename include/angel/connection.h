#ifndef __ANGEL_CONNECTION_H
#define __ANGEL_CONNECTION_H

#include <functional>
#include <memory>
#include <atomic>
#include <any> // only c++17
#include <queue>

#include <angel/channel.h>
#include <angel/buffer.h>
#include <angel/inet_addr.h>

namespace angel {

class evloop;
class connection;

typedef std::shared_ptr<connection> connection_ptr;

// Called after the connection is successfully established.
// Used by server or client.
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
    // The chl->fd() must be the fd returned by accept(2) or connect(2),
    // and the TCP connection has been successfully established.
    connection(size_t id, class channel *chl);
    virtual ~connection();

    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;

    size_t id() const { return conn_id; }
    evloop *get_loop() const { return loop; }
    const inet_addr& get_local_addr() const { return local_addr; }
    const inet_addr& get_peer_addr() const { return peer_addr; }
    bool is_reset_by_peer() const { return reset_by_peer; }
    bool is_connected() const { return state == Connected; }
    // Save the context needed for the connection. (used by user)
    //
    // There should not be a connection_ptr in the context,
    // otherwise it will cause a circular reference.
    //
    // If necessary, <connection*> can be.
    //
    // (not thread-safe)
    // Generally set_context() in connection_handler(),
    // and get_context() in message_handler() and close_handler().
    void set_context(std::any ctx) { context = std::move(ctx); }
    std::any& get_context() { return context; }
    // Set idle TTL(Time to Live) for connection. (thread-safe)
    void set_ttl(int64_t ms);
    // (thread-safe)
    // close(): Async close connection, return immediately.
    // close_wait(): Waiting until the connection is successfully closed.
    //
    // After that, there are the following two states:
    // Closed: Close the connection immediately.
    // Closing: Some data haven't been sent yet, so close after sending.
    void close();
    void close_wait();

    // (thread-safe)
    void send(std::string_view s);
    void send(const char *s, size_t len);
    void send(const void *v, size_t len);
    // (thread-safe)
    void format_send(const char *fmt, ...);
    // send_file() async-sends the specified file by zero copy. (thread-safe)
    void send_file(int fd, off_t offset, off_t count);
    // (thread-safe)
    // If you want to close fd after sending the file, you can do that.
    // send_file(), and then
    // set_send_complete_handler([]{ close(fd); })
    //
    // Or after send(), you can also do something with it.
    void set_send_complete_handler(const send_complete_handler_t handler);

    // Generally, they are only invoked by server and client,
    // and can not be called directly, unless you want to override
    // the handler set by server or client.
    void set_message_handler(message_handler_t handler);
    void set_close_handler(close_handler_t handler);
    void set_high_water_mark_handler(size_t size, high_water_mark_handler_t handler);
private:
    enum state_t { Connected, Closing, Closed };

    bool is_closing() const { return state == Closing; }
    bool is_closed() const { return state == Closed; }

    void handle_read();
    void handle_write();
    void handle_close(bool is_forced);
    void handle_error();
    void force_close_connection();
    void send_in_loop(const char *data, size_t len);
    void send_file_in_loop(int fd, off_t offset, off_t count);
    void set_ttl_timer();
    void update_ttl_timer();
    const char *get_state_str();

    virtual void handle_message();
    virtual ssize_t write(const char *data, size_t len);
    virtual ssize_t sendfile(int fd, off_t offset, off_t count);

    evloop *loop;
    channel *channel;
    const size_t conn_id;
    std::atomic<state_t> state;
    const inet_addr local_addr;
    const inet_addr peer_addr;
    std::any context;
    size_t ttl_timer_id;
    int64_t ttl_ms;

    struct file {
        int   fd;
        off_t offset;
        off_t count;
    };
    buffer input_buf;
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

    std::atomic_bool reset_by_peer;
    message_handler_t message_handler;
    close_handler_t close_handler;
    high_water_mark_handler_t high_water_mark_handler;
    size_t high_water_mark;

    friend class ssl_connection;
};

}

#endif // __ANGEL_CONNECTION_H
