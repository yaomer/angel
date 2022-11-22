#include <angel/connection.h>

#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include <angel/evloop.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

connection::connection(size_t id, evloop *loop, int sockfd)
    : conn_id(id),
    loop(loop),
    channel(new class channel(loop)),
    socket(new class socket(sockfd)),
    local_addr(sockops::get_local_addr(sockfd)),
    peer_addr(sockops::get_peer_addr(sockfd)),
    conn_state(state::connecting),
    ttl_timer_id(0), ttl_ms(0),
    send_id(1), next_id(1),
    high_water_mark(0)
{
    channel->set_fd(sockfd);
    channel->set_read_handler([this]{ this->handle_read(); });
    channel->set_write_handler([this]{ this->handle_write(); });
    channel->set_error_handler([this]{ this->handle_error(); });
    log_info("connection(id=%zu, fd=%d) is %s", id, sockfd, get_state_str());
}

connection::~connection()
{
    log_info("connection(id=%zu, fd=%d) is %s", conn_id, socket->fd(), get_state_str());
}

void connection::establish()
{
    loop->add_channel(channel);
    set_state(state::connected);
    log_info("connection(id=%zu, fd=%d) is %s", conn_id, socket->fd(), get_state_str());
    if (connection_handler) {
        connection_handler(shared_from_this());
    }
}

void connection::handle_read()
{
    ssize_t n = input_buf.read_fd(channel->fd());
    log_debug("Read (%zd) bytes from connection(id=%zu, fd=%d)", n, conn_id, channel->fd());
    if (n > 0) {
        if (message_handler) {
            handle_message();
        } else {
            // If the user does not set a message handler, discard all read data.
            input_buf.retrieve_all();
        }
    } else if (n == 0) {
        reset_by_peer = true;
        force_close_connection();
    } else {
        handle_error();
    }
    update_ttl_timer();
}

void connection::handle_message()
{
    message_handler(shared_from_this(), input_buf);
}

// Whenever the registered sockfd is writable,
// handle_write() is responsible for sending the unsent data to the peer.
void connection::handle_write()
{
    if (is_closed()) {
        log_warn("Unable to write, connection(id=%zu, fd=%d) is %s",
                 conn_id, channel->fd(), get_state_str());
        return;
    }
    if (!channel->is_writing()) return;

    if (!byte_stream_queue.empty() && byte_stream_queue.front().first == next_id) {
        auto& len = byte_stream_queue.front().second;
        ssize_t n = write(output_buf.peek(), len);
        if (n > 0) {
            len -= n;
            output_buf.retrieve(n);
            if (len == 0) {
                log_debug("Send complete for byte stream(send_id=%zu)", next_id);
                byte_stream_queue.pop();
                next_id++;
            }
        } else if (n == -2) {
            return;
        }
    }
    if (!send_file_queue.empty() && send_file_queue.front().first == next_id) {
        auto& f = send_file_queue.front().second;
        ssize_t n = sendfile(f.fd, f.offset, f.count);
        if (n > 0) {
            f.offset += n;
            f.count  -= n;
            if (f.count == 0) {
                log_debug("Send complete for file stream(send_id=%zu, fd=%d)", next_id, f.fd);
                send_file_queue.pop();
                next_id++;
            }
        } else if (n == -2) {
            return;
        }
    }
    if (!send_complete_handler_queue.empty() &&
        send_complete_handler_queue.front().first == next_id) {
        send_complete_handler_queue.front().second(shared_from_this());
        send_complete_handler_queue.pop();
        next_id++;
    }
    if (send_queue_is_empty() && send_complete_handler_queue.empty()) {
        channel->disable_write();
        if (is_closing()) force_close_connection();
    }
}

void connection::handle_close(bool is_forced)
{
    if (is_closed()) return;
    // We must cancel the ttl timer here, not when the connection is destructed;
    // otherwise, if the ttl timer exists, the lifetime of the connection will be extended.
    if (ttl_timer_id > 0) {
        loop->cancel_timer(ttl_timer_id);
        ttl_timer_id = 0;
    }
    if (!is_forced && !send_queue_is_empty()) {
        set_state(state::closing);
        return;
    }
    if (close_handler) {
        loop->run_in_loop([conn = shared_from_this()]{
                // Avoid calling close_handler() again in close_handler().
                auto close_handler = std::move(conn->close_handler);
                conn->close_handler = nullptr;
                close_handler(conn);
                });
    }
}

void connection::handle_error()
{
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        log_warn("connection(id=%zu, fd=%d): %s", conn_id, channel->fd(), strerrno());
    } else {
        log_error("connection(id=%zu, fd=%d): %s", conn_id, channel->fd(), strerrno());
        force_close_connection();
    }
}

void connection::send_in_loop(const char *data, size_t len)
{
    ssize_t n = 0;

    if (is_closed()) {
        log_warn("Unable to send, connection(id=%zu, fd=%d) is %s",
                 conn_id, channel->fd(), get_state_str());
        return;
    }
    log_debug("A new byte stream(len=%zu)", len);
    if (!channel->is_writing() && send_queue_is_empty()) {
        if ((n = write(data, len)) > 0) {
            len -= n;
        } else if (n == -2) {
            return;
        }
    }
    if (len > 0) {
        log_debug("Remaining (%zu) bytes, queued(send_id=%zu)...", len, send_id);
        output_buf.append(data + n, len);
        byte_stream_queue.emplace(send_id++, len);

        channel->enable_write();
        if (high_water_mark > 0 &&
            output_buf.readable() >= high_water_mark &&
            high_water_mark_handler) {
            loop->queue_in_loop([conn = shared_from_this()]{
                    conn->high_water_mark_handler(conn);
                    });
        }
    }
}

void connection::send_file_in_loop(int fd, off_t offset, off_t count)
{
    if (is_closed()) {
        log_warn("Unable to send file, connection(id=%zu, fd=%d) is %s",
                 conn_id, channel->fd(), get_state_str());
        return;
    }
    log_debug("A new file stream(fd=%d, offset=%lld, count=%lld)", fd, offset, count);
    if (!channel->is_writing() && send_queue_is_empty()) {
        ssize_t n = sendfile(fd, offset, count);
        if (n > 0) {
            offset += n;
            count  -= n;
        } else if (n == -2) {
            return;
        }
    }
    if (count > 0) {
        file f;
        f.fd     = fd;
        f.offset = offset;
        f.count  = count;
        log_debug("Remaining (%lld, %lld) for file(fd=%d), queued(send_id=%zu)...",
                  offset, count, fd, send_id);
        send_file_queue.emplace(send_id++, std::move(f));
        channel->enable_write();
    }
}

void connection::set_send_complete_handler(const send_complete_handler_t handler)
{
    loop->run_in_loop([conn = shared_from_this(), handler = std::move(handler)]{
            conn->send_complete_handler_queue.emplace(conn->send_id++, handler);
            conn->channel->enable_write();
            });
}

ssize_t connection::write(const char *data, size_t len)
{
    int fd = channel->fd();
    ssize_t n = ::write(fd, data, len);
    log_debug("Write (%zd) bytes to connection(id=%zu, fd=%d)", n, conn_id, fd);
    if (n < 0) {
        handle_error();
        return is_closed() ? -2 : -1;
    }
    return n;
}

ssize_t connection::sendfile(int fd, off_t offset, off_t count)
{
    int sockfd = channel->fd();
    ssize_t n = sockops::sendfile(fd, sockfd, offset, count);
    log_debug("Send file(fd=%d) (%lld, %lld) (%zu) bytes to connection(id=%zu, fd=%d)",
              fd, offset, offset + n, n, conn_id, sockfd);
    if (n < 0) {
        handle_error();
        return is_closed() ? -2 : -1;
    }
    return n;
}

void connection::send(const char *s, size_t len)
{
    if (loop->is_io_loop_thread()) {
        // You can send it directly on this thread.
        send_in_loop(s, len);
    } else {
        // Delayed sending across threads must copy the data to prevent data failure.
        loop->queue_in_loop([this, message = std::string(s, len)]{
                this->send_in_loop(message.data(), message.size());
                });
    }
    update_ttl_timer();
}

void connection::send(std::string_view s)
{
    send(s.data(), s.size());
}

void connection::send(const void *v, size_t len)
{
    send(reinterpret_cast<const char*>(v), len);
}

void connection::format_send(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    auto res = format(fmt, ap);
    va_end(ap);
    send(res.buf, res.len);
    if (res.alloced) delete []res.buf;
}

void connection::send_file(int fd, off_t offset, off_t count)
{
    loop->run_in_loop([this, fd, offset, count]{
            this->send_file_in_loop(fd, offset, count);
            });
    update_ttl_timer();
}

void connection::set_ttl(int64_t ms)
{
    if (ms <= 0) return;
    ttl_ms = ms;
    if (ttl_timer_id > 0) loop->cancel_timer(ttl_timer_id);
    set_ttl_timer();
}

void connection::update_ttl_timer()
{
    if (ttl_timer_id == 0) return;
    loop->cancel_timer(ttl_timer_id);
    set_ttl_timer();
}

void connection::set_ttl_timer()
{
    ttl_timer_id = loop->run_after(ttl_ms, [conn = shared_from_this()]{
            conn->close();
            });
}

const char *connection::get_state_str()
{
    switch (conn_state) {
    case state::connecting: return "<Connecting>";
    case state::connected: return "<Connected>";
    case state::closing: return "<Closing>";
    case state::closed: return "<Closed>";
    default: return "<None>";
    }
}

}
