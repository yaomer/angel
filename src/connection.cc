#include <angel/connection.h>

#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include <angel/evloop.h>
#include <angel/inet_addr.h>
#include <angel/socket.h>
#include <angel/channel.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

connection::connection(size_t id, evloop *loop, int sockfd)
    : conn_id(id),
    loop(loop),
    conn_channel(new channel(loop)),
    conn_socket(new socket(sockfd)),
    local_addr(sockops::get_local_addr(sockfd)),
    peer_addr(sockops::get_peer_addr(sockfd)),
    conn_state(state::connecting),
    ttl_timer_id(0),
    ttl_ms(0),
    high_water_mark(0)
{
    conn_channel->set_fd(sockfd);
    conn_channel->set_read_handler([this]{ this->handle_read(); });
    conn_channel->set_write_handler([this]{ this->handle_write(); });
    conn_channel->set_error_handler([this]{ this->handle_error(); });
    log_info("connection(id=%d, fd=%d) is %s", id, sockfd, get_state_str());
}

connection::~connection()
{
    if (ttl_timer_id > 0) {
        loop->cancel_timer(ttl_timer_id);
    }
    log_info("connection(id=%d, fd=%d) is %s", conn_id, conn_socket->fd(), get_state_str());
}

void connection::establish()
{
    loop->add_channel(conn_channel);
    set_state(state::connected);
    log_info("connection(id=%d, fd=%d) is %s", conn_id, conn_socket->fd(), get_state_str());
    if (connection_handler)
        connection_handler(shared_from_this());
}

void connection::handle_read()
{
    ssize_t n = input_buf.read_fd(conn_channel->fd());
    log_debug("read %zd bytes from connection(id=%d, fd=%d)", n, conn_id, conn_channel->fd());
    if (n > 0) {
        if (message_handler) {
            message_handler(shared_from_this(), input_buf);
        } else {
            // 如果用户未设置消息回调，就丢掉所有读到的数据
            input_buf.retrieve_all();
        }
    } else if (n == 0) {
        force_close_connection();
    } else {
        handle_error();
    }
    update_ttl_timer_if_needed();
}

// 每当关注的sockfd可写时，由handle_write()负责将未发送完的数据发送过去
void connection::handle_write()
{
    if (conn_state == state::closed) {
        log_warn("unable to write, connection(id=%zu, fd=%d) is %s",
                conn_id, conn_channel->fd(), get_state_str());
        return;
    }
    if (conn_channel->is_writing()) {
        ssize_t n = write(conn_channel->fd(), output_buf.peek(), output_buf.readable());
        if (n >= 0) {
            output_buf.retrieve(n);
            if (output_buf.readable() == 0) {
                conn_channel->disable_write();
                if (write_complete_handler)
                    loop->queue_in_loop([conn = shared_from_this()]{
                            conn->write_complete_handler(conn);
                            });
                if (conn_state == state::closing)
                    force_close_connection();
            }
        } else {
            log_warn("write to connection(id=%d, fd=%d): %s", conn_id, conn_channel->fd(), strerrno());
            // 对端尚未与我们建立连接或者对端已关闭连接
            if (errno == ECONNRESET || errno == EPIPE) {
                force_close_connection();
                return;
            }
        }
    }
}

void connection::handle_close(bool is_forced)
{
    log_info("connection(id=%d, fd=%d) is %s", conn_id, conn_channel->fd(), get_state_str());
    if (conn_state == state::closed) return;
    if (!is_forced && output_buf.readable() > 0) {
        set_state(state::closing);
        return;
    }
    if (close_handler)
        loop->run_in_loop([conn = shared_from_this()]{
                conn->close_handler(conn);
                });
}

void connection::handle_error()
{
    log_error("connection(id=%d, fd=%d): %s", conn_id, conn_channel->fd(), strerrno());
    // 对端已关闭连接
    if (errno == ECONNRESET)
        force_close_connection();
}

void connection::send_in_loop(const char *data, size_t len)
{
    ssize_t n = 0;
    size_t remain_bytes = len;

    if (conn_state == state::closed) {
        log_warn("unable to send, connection(id=%zu, fd=%d) is %s",
                conn_id, conn_channel->fd(), get_state_str());
        return;
    }
    if (!conn_channel->is_writing() && output_buf.readable() == 0) {
        n = write(conn_channel->fd(), data, len);
        log_debug("write %zd bytes to connection(id=%d, fd=%d)", n, conn_id, conn_channel->fd());
        if (n >= 0) {
            remain_bytes = len - n;
            if (remain_bytes == 0) {
                if (write_complete_handler)
                    loop->queue_in_loop([conn = shared_from_this()]{
                            conn->write_complete_handler(conn);
                            });
                if (conn_state == state::closing)
                    force_close_connection();
            }
        } else {
            log_warn("write to connection(id=%d, fd=%d): %s", conn_id, conn_channel->fd(), strerrno());
            if (errno == ECONNRESET || errno == EPIPE) {
                force_close_connection();
                return;
            }
        }
    }
    if (remain_bytes > 0) {
        output_buf.append(data + n, len - n);
        if (!conn_channel->is_writing())
            conn_channel->enable_write();
        if (high_water_mark > 0 &&
            output_buf.readable() >= high_water_mark &&
            high_water_mark_handler) {
            loop->queue_in_loop([conn = shared_from_this()]{
                    conn->high_water_mark_handler(conn);
                    });
        }
    }
}

void connection::send(const char *s, size_t len)
{
    if (loop->is_io_loop_thread()) {
        // 在本线程直接发送即可
        send_in_loop(s, len);
    } else {
        // 跨线程必须将数据拷贝一份，防止数据失效
        loop->run_in_loop([this, message = std::string(s, len)]{
                this->send_in_loop(message.data(), message.size());
                });
    }
    update_ttl_timer_if_needed();
}

void connection::send(std::string_view s)
{
    send(s.data(), s.size());
}

void connection::send(const void *v, size_t len)
{
    send(reinterpret_cast<const char*>(v), len);
}

static thread_local char conn_format_send_buf[65536];

void connection::format_send(const char *fmt, ...)
{
    va_list ap, ap1;
    va_start(ap, fmt);
    va_copy(ap1, ap);
    int len = vsnprintf(nullptr, 0, fmt, ap1);
    va_end(ap1);
    char *buf;
    bool is_alloced = false;
    if (len < sizeof(conn_format_send_buf)) {
        buf = conn_format_send_buf;
    } else {
        buf = new char[len + 1];
        is_alloced = true;
    }
    vsnprintf(buf, len + 1, fmt, ap);
    va_end(ap);
    send(buf, len);
    if (is_alloced) delete []buf;
}

int connection::send_file(int fd)
{
    return sockops::send_file(fd, conn_channel->fd());
}

void connection::update_ttl_timer_if_needed()
{
    if (ttl_ms <= 0) return;
    loop->cancel_timer(ttl_timer_id);
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
