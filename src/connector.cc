#include <errno.h>
#include <unistd.h>

#include "evloop.h"
#include "socket.h"
#include "sockops.h"
#include "connector.h"
#include "logger.h"

using namespace angel;

connector_t::connector_t(evloop *loop, inet_addr peer_addr)
    : loop(loop),
    peer_addr(peer_addr),
    connect_channel(new channel(loop)),
    has_connected(false),
    wait_retry(false)
{
}

void connector_t::connect()
{
    wait_retry = false;
    log_info("connect -> host %s", peer_addr.to_host());
    int sockfd = sockops::socket();
    sockops::set_nonblock(sockfd);
    int ret = sockops::connect(sockfd, &peer_addr.addr());
    connect_channel->set_fd(sockfd);
    loop->add_channel(connect_channel);
    if (ret == 0) {
        // 通常如果服务端和客户端在同一台主机，连接会立即建立
        connected(sockfd);
    } else if (ret < 0) {
        if (errno == EINPROGRESS) {
            // 连接正在建立
            connecting(sockfd);
        }
    }
}

void connector_t::connecting(int sockfd)
{
    log_debug("fd=%d is connecting", sockfd);
    auto check_handler = [this, sockfd]{ this->check(sockfd); };
    connect_channel->set_read_handler(check_handler);
    connect_channel->set_write_handler(check_handler);
    connect_channel->enable_write();
}

void connector_t::connected(int sockfd)
{
    if (has_connected) return;
    log_debug("fd=%d is connected", sockfd);
    loop->remove_channel(connect_channel);
    connect_channel.reset();
    if (new_connection_handler) {
        new_connection_handler(sockfd);
        has_connected = true;
    }
}

// 在Mac OS上使用poll判断sockfd的状态，好像有时会可读，有时会可写
// 所以我们在可读可写情况下都使用getsockopt来获取sockfd的状态

// 如果一个socket fd上出现了错误，那么在close(fd)之前，只能通过
// getsockopt()获取一次错误，之后便不会再触发

void connector_t::check(int sockfd)
{
    if (wait_retry) return;
    int err = sockops::get_socket_error(sockfd);
    if (err) {
        log_error("connect: %s, try to reconnect after %d s", util::strerr(err), retry_interval);
        retry(sockfd);
        return;
    }
    connected(sockfd);
}

void connector_t::retry(int sockfd)
{
    loop->remove_channel(connect_channel);
    connect_channel.reset();
    loop->run_after(1000 * retry_interval, [this]{ this->connect(); });
    wait_retry = true;
    ::close(sockfd);
}
