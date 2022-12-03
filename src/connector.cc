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
    connect_channel(nullptr),
    retry_timer_id(0)
{
}

connector_t::~connector_t()
{
    shutdown();
}

void connector_t::connect()
{
    int sockfd = sockops::socket(protocol);
    sockops::set_nonblock(sockfd);
    int ret = sockops::connect(sockfd, &peer_addr.addr());

    // Fatal error.
    if (ret < 0 && errno != EINPROGRESS) {
        close(sockfd);
        log_error("connect: %s", util::strerrno());
        if (onfail) onfail();
        return;
    }

    Assert(!connect_channel);
    connect_channel = new channel(loop, sockfd);
    log_info("(fd=%d) connect -> host (%s)", sockfd, peer_addr.to_host());

    if (ret == 0) {
        connect_channel->add();
        // Usually if the server and client are on the same host,
        // the connection will be established immediately.
        loop->run_in_loop([this]{ this->connected(); });
    } else {
        // The socket is non-blocking and the connection cannot be completed immediately.
        // It is possible to select(2) or poll(2) for completion.
        //
        // If connect(2) completed successfully, the socket will become writable;
        // If an error occurs, it will become readable and writable.
        // So we only need to monitor writable event.
        //
        // After select(2) indicates writability, we use getsockopt(2) to determine
        // whether connect(2) completed successfully or unsuccessfully.
        connect_channel->set_write_handler([this]{ this->check(); });
        connect_channel->enable_write();
        connect_channel->add();
    }
}

void connector_t::connected()
{
    if (onconnect) {
        if (keep_reconnect){
            loop->cancel_timer(retry_timer_id);
        }
        // Transfer the ownership of the channel to the upper layer.
        onconnect(connect_channel);
        connect_channel = nullptr;
    } else {
        shutdown();
    }
}

// If there is an error on a socket fd, before close(fd),
// the error can only be gotten once through getsockopt(),
// and then it will not be triggered again.
void connector_t::check()
{
    int err = sockops::get_socket_error(connect_channel->fd());
    if (err) {
        log_error("connect(%s): %s", peer_addr.to_host(), util::strerr(err));
        shutdown();
        if (keep_reconnect) retry();
        else if (onfail) onfail();
    } else {
        connected();
    }
}

void connector_t::retry()
{
    log_error("Try to reconnect -> (%s) after %d ms", peer_addr.to_host(), retry_interval);
    retry_timer_id = loop->run_after(retry_interval, [this]{ this->connect(); });
}

void connector_t::shutdown()
{
    if (keep_reconnect) {
        loop->cancel_timer(retry_timer_id);
    }
    if (connect_channel) {
        connect_channel->remove();
        connect_channel = nullptr;
    }
}

}
