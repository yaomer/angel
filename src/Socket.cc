#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include "Socket.h"
#include "InetAddr.h"
#include "LogStream.h"

using namespace Angel;

Socket::~Socket()
{
    ::close(_sockfd);
}

void Socket::socket()
{
    if ((_sockfd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
        LOG_FATAL << strerrno();
}

void Socket::bind(InetAddr& _inetAddr)
{
    if (::bind(_sockfd,
               reinterpret_cast<struct sockaddr*>(&_inetAddr.inetAddr()),
               sizeof(_inetAddr.inetAddr())) < 0)
        LOG_FATAL << strerrno();
}

void Socket::listen()
{
    if (::listen(_sockfd, 1024) < 0)
        LOG_FATAL << strerrno();
}

// accept()和connect()都可能涉及到非阻塞问题，
// 所以交由上层应用自行处理返回值
int Socket::accept()
{
    InetAddr _inetAddr;
    socklen_t socklen = sizeof(_inetAddr.inetAddr());
    return ::accept(_sockfd,
                    reinterpret_cast<struct sockaddr*>(&_inetAddr.inetAddr()),
                    &socklen);
}

int Socket::connect(InetAddr& _inetAddr)
{
    return ::connect(_sockfd,
                    reinterpret_cast<struct sockaddr*>(&_inetAddr.inetAddr()),
                    sizeof(_inetAddr.inetAddr()));
}

void Socket::setNonBlock()
{
    setNonBlock(_sockfd);
}

void Socket::setNoDelay(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (setsockopt(_sockfd, SOL_SOCKET, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        LOG_FATAL << "setsockopt error: " << strerrno();
    LOG_INFO << (on ? "enable" : "disable") << " TCP_NODELAY";
}

void Socket::setReuseAddr(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        LOG_FATAL << "setsockopt error: " << strerrno();
    LOG_INFO << (on ? "enable" : "disable") << " SO_REUSEADDR";
}

void Socket::setReusePort(bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        LOG_FATAL << "setsockopt error: " << strerrno();
    LOG_INFO << (on ? "enable" : "disable") << " SO_REUSEPORT";
}

void Socket::socketpair(int fd[])
{
    if (::socketpair(AF_LOCAL, SOCK_STREAM, 0, fd) < 0)
        LOG_FATAL << "socketpair error: " << strerrno();
}

int Socket::socketError()
{
    socklen_t err;
    socklen_t len = sizeof(err);
    if (::getsockopt(_sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        LOG_FATAL << "getsockopt error: " << strerrno();
    return static_cast<int>(err);
}

void Socket::setNonBlock(int fd)
{
    int oflag = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, oflag | O_NONBLOCK);
}
