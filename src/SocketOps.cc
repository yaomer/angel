#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "SocketOps.h"
#include "LogStream.h"

using namespace Angel;

struct sockaddr *SocketOps::sockaddr_cast(struct sockaddr_in *addr)
{
    return reinterpret_cast<struct sockaddr *>(addr);
}

int SocketOps::socket()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        LOG_FATAL << "socket(): " << strerrno();
    return sockfd;
}

void SocketOps::bind(int sockfd, struct sockaddr_in *addr)
{
    if (::bind(sockfd, sockaddr_cast(addr), sizeof(*addr)) < 0)
        LOG_FATAL << "bind(): " << strerrno();
}

void SocketOps::listen(int sockfd)
{
    if (::listen(sockfd, 1024) < 0)
        LOG_FATAL << "listen(): " << strerrno();
}

int SocketOps::accept(int sockfd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    return ::accept(sockfd, sockaddr_cast(&addr), &len);
}

int SocketOps::connect(int sockfd, struct sockaddr_in *addr)
{
    return ::connect(sockfd, sockaddr_cast(addr), sizeof(*addr));
}

void SocketOps::setnonblock(int sockfd)
{
    int oflag = ::fcntl(sockfd, F_GETFL, 0);
    ::fcntl(sockfd, F_SETFL, oflag | O_NONBLOCK);
}

void SocketOps::socketpair(int sockfd[])
{
    if (::socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd) < 0)
        LOG_FATAL << "socketpair(): " << strerrno();
}

struct sockaddr_in SocketOps::getLocalAddr(int sockfd)
{
    struct sockaddr_in localAddr;
    bzero(&localAddr, sizeof(localAddr));
    socklen_t len = static_cast<socklen_t>(sizeof(localAddr));
    if (::getsockname(sockfd, sockaddr_cast(&localAddr), &len) < 0)
        LOG_FATAL << "getsockname(): " << strerrno();
    return localAddr;
}

struct sockaddr_in SocketOps::getPeerAddr(int sockfd)
{
    struct sockaddr_in peerAddr;
    bzero(&peerAddr, sizeof(peerAddr));
    socklen_t len = static_cast<socklen_t>(sizeof(peerAddr));
    if (::getpeername(sockfd, sockaddr_cast(&peerAddr), &len) < 0)
        LOG_FATAL << "getpeername(): " << strerrno();
    return peerAddr;
}

int SocketOps::getSocketError(int sockfd)
{
    socklen_t err;
    socklen_t len = sizeof(err);
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        LOG_FATAL << "getsockopt(): " << strerrno();
    return static_cast<int>(err);
}

int SocketOps::toHostIpPort(in_port_t port)
{
    return ntohs(port);
}

namespace Angel {

    __thread char _ipaddr[32];
}

const char *SocketOps::toHostIpAddr(struct in_addr *addr)
{
    if (inet_ntop(AF_INET, addr, _ipaddr, sizeof(_ipaddr)) == nullptr)
        LOG_FATAL << "inet_ntop(): " << strerrno();
    return _ipaddr;
}
