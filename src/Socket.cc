#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include "Socket.h"
#include "SocketOps.h"
#include "InetAddr.h"
#include "LogStream.h"

using namespace Angel;

Socket::~Socket()
{
    ::close(_sockfd);
}

void Socket::socket()
{
    _sockfd = SocketOps::socket();
}

void Socket::bind(InetAddr& _inetAddr)
{
    SocketOps::bind(_sockfd, &_inetAddr.inetAddr());
}

void Socket::listen()
{
    SocketOps::listen(_sockfd);
}

int Socket::accept()
{
    return SocketOps::accept(_sockfd);
}

int Socket::connect(InetAddr& _inetAddr)
{
    return SocketOps::connect(_sockfd, &_inetAddr.inetAddr());
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
