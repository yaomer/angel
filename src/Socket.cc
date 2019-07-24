#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>
#include "Socket.h"
#include "LogStream.h"
#include "config.h"

using namespace Angel;

Socket::Socket(int fd)
    : _sockfd(fd)
{
    logInfo("[Socket::ctor, fd = %d]", fd);
}

Socket::~Socket()
{
    logInfo("[Socket::dtor, close(%d)]", _sockfd);
    ::close(_sockfd);
}

void Socket::setKeepAlive(bool on)
{
    socklen_t opt = on ? 1 : 0;
#ifdef _ANGEL_HAVE_TCP_KEEPALIVE
    int optval = TCP_KEEPALIVE;
#elif _ANGEL_HAVE_SO_KEEPALIVE
    int optval = SO_KEEPALIVE;
#endif
    if (::setsockopt(_sockfd, SOL_SOCKET, optval, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> TCP_KEEPALIVE]: %s", strerrno());
    logInfo("%s TCP_KEEPALIVE", (on ? "enable" : "disable"));
}

void Socket::setNoDelay(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(_sockfd, SOL_SOCKET, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> TCP_NODELAY]: %s", strerrno());
    logInfo("%s TCP_NODELAY", (on ? "enable" : "disable"));
}

void Socket::setReuseAddr(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> SO_REUSEADDR]: %s", strerrno());
    logInfo("%s SO_REUSEADDR", (on ? "enable" : "disable"));
}

void Socket::setReusePort(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> SO_REUSEPORT]: %s", strerrno());
    logInfo("%s SO_REUSEPORT", (on ? "enable" : "disable"));
}
