#include "connector.h"

#include <unistd.h>
#include <errno.h>

#include <angel/evloop.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

connector_t::connector_t(evloop *loop, inet_addr peer_addr)
    : loop(loop),
    peer_addr(peer_addr),
    has_connected(false),
    retry_timer_id(0),
    wait_retry(false),
    sockfd(-1)
{
}

connector_t::~connector_t()
{
    if (wait_retry) {
        loop->cancel_timer(retry_timer_id);
    }
    if (!wait_retry && !has_connected) {
        loop->remove_channel(sockfd);
        loop->run_in_loop([fd = sockfd]{ close(fd); });
    }
}

void connector_t::connect()
{
    wait_retry = false;
    sockfd = sockops::socket(protocol);
    sockops::set_nonblock(sockfd);
    int ret = sockops::connect(sockfd, &peer_addr.addr());
    auto chl = std::make_shared<channel>(loop);
    chl->set_fd(sockfd);
    loop->add_channel(chl);
    log_info("(fd=%d) connect -> host (%s)", sockfd, peer_addr.to_host());
    if (ret == 0) {
        // Usually if the server and client are on the same host,
        // the connection will be established immediately.
        connected();
        return;
    }
    if (errno != EINPROGRESS) {
        log_error("connect(): %s", util::strerrno());
        return;
    }
    // The socket is non-blocking and the connection cannot be completed immediately.
    // It is possible to select(2) or poll(2) for completion by selecting the socket for writing.
    log_debug("(fd=%d) connecting...", sockfd);
    auto check_handler = [this]{ this->check(); };
    chl->set_read_handler(check_handler);
    chl->set_write_handler(check_handler);
    chl->enable_write();
}

void connector_t::connected()
{
    if (has_connected) return;
    if (wait_retry) {
        loop->cancel_timer(retry_timer_id);
    }
    loop->remove_channel(sockfd);
    has_connected = true;
    new_connection_handler(sockfd);
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
        log_error("connect(fd=%d): %s", sockfd, util::strerr(err));
        retry();
        return;
    }
    connected();
}

void connector_t::retry()
{
    loop->remove_channel(sockfd);
    loop->run_in_loop([fd = sockfd]{ close(fd); });
    log_error("try to reconnect -> (%s) after %d ms", peer_addr.to_host(), retry_interval);
    retry_timer_id = loop->run_after(retry_interval, [this]{ this->connect(); });
    wait_retry = true;
}

}
