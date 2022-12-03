#include "listener.h"

#include <unistd.h>
#include <fcntl.h>

#include <angel/evloop.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

listener_t::listener_t(evloop *loop, inet_addr listen_addr)
    : loop(loop),
    listen_channel(nullptr),
    listen_addr(listen_addr),
    idle_fd(open("/dev/null", O_RDONLY | O_CLOEXEC))
{
}

listener_t::~listener_t()
{
    listen_channel->remove();
}

void listener_t::listen()
{
    int fd = sockops::socket();
    sockops::set_reuseaddr(fd, true);
    sockops::set_nodelay(fd, nodelay);
    sockops::set_keepalive(fd, keepalive);
    sockops::set_keepalive_idle(fd, keepalive_idle);
    sockops::set_keepalive_intvl(fd, keepalive_intvl);
    sockops::set_keepalive_probes(fd, keepalive_probes);
    sockops::set_nonblock(fd);
    sockops::bind(fd, &listen_addr.addr());
    sockops::listen(fd);

    listen_channel = new channel(loop, fd);
    listen_channel->set_read_handler([this]{ this->handle_accept(); });
    listen_channel->add();
}

void listener_t::handle_accept()
{
    int connfd = sockops::accept(listen_channel->fd());
    if (connfd >= 0) {
        log_info("Accept a new connection(fd=%d)", connfd);
        sockops::set_nonblock(connfd);
        if (onaccept) onaccept(connfd);
        else close(connfd);
        return;
    }
    switch (errno) {
    case EINTR:
    case EWOULDBLOCK: // BSD
    case EPROTO: // SVR4
    case ECONNABORTED: // POSIX
        break;
    case EMFILE:
        close(idle_fd);
        connfd = sockops::accept(listen_channel->fd());
        close(connfd);
        idle_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    default:
        log_error("accept: %s", util::strerrno());
        break;
    }
}

}
