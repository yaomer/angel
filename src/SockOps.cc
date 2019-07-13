#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;

struct sockaddr *SockOps::sockaddr_cast(struct sockaddr_in *addr)
{
    return reinterpret_cast<struct sockaddr *>(addr);
}

int SockOps::socket()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        logFatal("socket: %s", strerrno());
    return sockfd;
}

void SockOps::bind(int sockfd, struct sockaddr_in *addr)
{
    if (::bind(sockfd, sockaddr_cast(addr), sizeof(*addr)) < 0)
        logFatal("bind: %s", strerrno());
}

void SockOps::listen(int sockfd)
{
    if (::listen(sockfd, 1024) < 0)
        logFatal("listen: %s", strerrno());
}

int SockOps::accept(int sockfd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int connfd = ::accept(sockfd, sockaddr_cast(&addr), &len);
    return connfd;
}

int SockOps::connect(int sockfd, struct sockaddr_in *addr)
{
    int ret = ::connect(sockfd, sockaddr_cast(addr), sizeof(*addr));
    return ret;
}

void SockOps::setnonblock(int sockfd)
{
    int oflag = ::fcntl(sockfd, F_GETFL, 0);
    ::fcntl(sockfd, F_SETFL, oflag | O_NONBLOCK);
}

void SockOps::socketpair(int sockfd[])
{
    if (::socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd) < 0)
        logFatal("socketpair: %s", strerrno());
}

struct sockaddr_in SockOps::getLocalAddr(int sockfd)
{
    struct sockaddr_in localAddr;
    bzero(&localAddr, sizeof(localAddr));
    socklen_t len = static_cast<socklen_t>(sizeof(localAddr));
    if (::getsockname(sockfd, sockaddr_cast(&localAddr), &len) < 0)
        logError("getsockname: %s", strerrno());
    return localAddr;
}

struct sockaddr_in SockOps::getPeerAddr(int sockfd)
{
    struct sockaddr_in peerAddr;
    bzero(&peerAddr, sizeof(peerAddr));
    socklen_t len = static_cast<socklen_t>(sizeof(peerAddr));
    if (::getpeername(sockfd, sockaddr_cast(&peerAddr), &len) < 0)
        logError("getpeername: %s", strerrno());
    return peerAddr;
}

int SockOps::getSocketError(int sockfd)
{
    socklen_t err;
    socklen_t len = sizeof(err);
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        logError("[getsockopt -> SO_ERROR]: %s", strerrno());
    return static_cast<int>(err);
}

int SockOps::toHostIpPort(in_port_t port)
{
    return ntohs(port);
}

namespace Angel {

    thread_local char _ipaddr[32];
}

const char *SockOps::toHostIpAddr(struct in_addr *addr)
{
    if (inet_ntop(AF_INET, addr, _ipaddr, sizeof(_ipaddr)) == nullptr)
        logError("inet_ntop: %s", strerrno());
    return _ipaddr;
}
