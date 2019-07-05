#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include "Socket.h"
#include "LogStream.h"

using namespace Angel;

Socket::Socket(int fd)
    : _sockfd(fd)
{
    LOG_INFO << "[Socket::ctor, fd = " << _sockfd << "]";
}

Socket::~Socket()
{
    LOG_INFO << "[Socket::dtor, close(" << _sockfd << ")]";
    ::close(_sockfd);
}

void Socket::setKeepAlive(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(_sockfd, SOL_SOCKET, TCP_KEEPALIVE, &opt, sizeof(opt)) < 0)
        LOG_FATAL << "setsockopt(): " << strerrno();
    LOG_INFO << (on ? "enable" : "disable") << " TCP_KEEPALIVE";
}

void Socket::setNoDelay(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(_sockfd, SOL_SOCKET, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        LOG_FATAL << "setsockopt(): " << strerrno();
    LOG_INFO << (on ? "enable" : "disable") << " TCP_NODELAY";
}

void Socket::setReuseAddr(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        LOG_FATAL << "setsockopt(): " << strerrno();
    LOG_INFO << (on ? "enable" : "disable") << " SO_REUSEADDR";
}

void Socket::setReusePort(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        LOG_FATAL << "setsockopt(): " << strerrno();
    LOG_INFO << (on ? "enable" : "disable") << " SO_REUSEPORT";
}
