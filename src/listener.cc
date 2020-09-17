#include <unistd.h>
#include <fcntl.h>

#include "listener.h"
#include "evloop.h"
#include "sockops.h"
#include "logger.h"

using namespace angel;

listener_t::listener_t(evloop *loop, inet_addr listen_addr)
    : loop(loop),
    listen_channel(new channel(loop)),
    listen_socket(sockops::socket()),
    listen_addr(listen_addr),
    idle_fd(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
}

void listener_t::listen()
{
    int fd = listen_socket.fd();
    sockops::set_reuseaddr(fd, true);
    sockops::set_nodelay(fd, true);
    sockops::set_keepalive(fd, true);
    sockops::set_nonblock(fd);
    sockops::bind(fd, &listen_addr.addr());
    sockops::listen(fd);
    listen_channel->set_fd(fd);
    listen_channel->set_read_handler([this]{ this->handle_accept(); });
    loop->add_channel(listen_channel);
    log_debug("listen port is %d",  listen_addr.to_host_port());
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
    log_info("accept a new connection, fd = %d", connfd);
    sockops::set_nonblock(connfd);
    if (new_connection_handler)
        new_connection_handler(connfd);
    else
        ::close(connfd);
}
