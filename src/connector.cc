#include "connector.h"

#include <unistd.h>
#include <errno.h>

#include <angel/evloop.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

connector_t::connector_t(evloop *loop, inet_addr peer_addr,
                         const new_connection_handler_t handler,
                         int64_t retry_interval_ms,
                         std::string_view protocol)
    : loop(loop),
    peer_addr(peer_addr),
    connect_channel(new channel(loop)),
    new_connection_handler(handler),
    has_connected(false),
    retry_timer_id(0),
    wait_retry(false),
    sockfd(-1),
    retry_interval(retry_interval_ms),
    protocol(protocol)
{
}

connector_t::~connector_t()
{
    if (wait_retry) {
        loop->cancel_timer(retry_timer_id);
    }
    if (!wait_retry && !has_connected) {
        loop->remove_channel(connect_channel);
        close(sockfd);
    }
}

void connector_t::connect()
{
    wait_retry = false;
    sockfd = sockops::socket(protocol);
    sockops::set_nonblock(sockfd);
    int ret = sockops::connect(sockfd, &peer_addr.addr());
    connect_channel->set_fd(sockfd);
    loop->add_channel(connect_channel);
    log_info("connect(fd=%d) -> host (%s)", sockfd, peer_addr.to_host());
    if (ret == 0) {
        // Usually if the server and client are on the same host,
        // the connection will be established immediately.
        connected();
    } else if (ret < 0) {
        if (errno == EINPROGRESS) {
            connecting();
        }
    }
}

void connector_t::connecting()
{
    log_debug("connector(fd=%d) is connecting", sockfd);
    auto check_handler = [this]{ this->check(); };
    connect_channel->set_read_handler(check_handler);
    connect_channel->set_write_handler(check_handler);
    connect_channel->enable_write();
}

void connector_t::connected()
{
    if (has_connected) return;
    log_debug("connector(fd=%d) is connected", sockfd);
    loop->remove_channel(connect_channel);
    connect_channel.reset();
    new_connection_handler(sockfd);
    has_connected = true;
}

// Using poll to judge the status of sockfd on Mac OS
// seems to be sometimes readable and sometimes writable,
// so we use getsockopt() to get the status of sockfd
// in both readable and writable situations.

// If there is an error on a socket fd, before close(fd),
// the error can only be gotten once through getsockopt(),
// and then it will not be triggered again.

void connector_t::check()
{
    if (wait_retry) return;
    int err = sockops::get_socket_error(sockfd);
    if (err) {
        log_error("connect(fd=%d): %s, try to reconnect after %d ms",
                  sockfd, util::strerr(err), retry_interval);
        retry();
        return;
    }
    connected();
}

void connector_t::retry()
{
    loop->remove_channel(connect_channel);
    retry_timer_id = loop->run_after(retry_interval, [this]{ this->connect(); });
    wait_retry = true;
    close(sockfd);
}

}
