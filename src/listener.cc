#include "listener.h"

#include <unistd.h>
#include <fcntl.h>

#include <angel/evloop.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

listener_t::listener_t(evloop *loop, inet_addr listen_addr,
                       const new_connection_handler_t handler)
    : loop(loop),
    listen_channel(new channel(loop)),
    listen_socket(sockops::socket()),
    listen_addr(listen_addr),
    new_connection_handler(handler),
    idle_fd(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
}

listener_t::~listener_t() = default;

void listener_t::listen()
{
    int fd = listen_socket.fd();
    sockops::set_reuseaddr(fd, true);
    sockops::set_nodelay(fd, nodelay);
    sockops::set_keepalive(fd, keepalive);
    sockops::set_keepalive_idle(fd, keepalive_idle);
    sockops::set_keepalive_intvl(fd, keepalive_intvl);
    sockops::set_keepalive_probes(fd, keepalive_probes);
    sockops::set_nonblock(fd);
    sockops::bind(fd, &listen_addr.addr());
    sockops::listen(fd);
    listen_channel->set_fd(fd);
    listen_channel->set_read_handler([this]{ this->handle_accept(); });
    loop->add_channel(listen_channel);
}

void listener_t::handle_accept()
{
    int connfd = sockops::accept(listen_socket.fd());
    if (connfd < 0) {
        switch (errno) {
        case EINTR:
        case EWOULDBLOCK: // BSD
        case EPROTO: // SVR4
        case ECONNABORTED: // POSIX
            break;
        case EMFILE:
            ::close(idle_fd);
            connfd = sockops::accept(listen_socket.fd());
            ::close(connfd);
            idle_fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            break;
        default:
            log_error("accept: %s", util::strerrno());
            break;
        }
        return;
    }
    log_info("accept a new connection(fd=%d)", connfd);
    sockops::set_nonblock(connfd);
    new_connection_handler(connfd);
}

}
