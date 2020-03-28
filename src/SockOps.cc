#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string.h>

#include "SockOps.h"
#include "LogStream.h"
#include "config.h"

using namespace Angel;

static inline struct sockaddr *sockaddr_cast(struct sockaddr_in *addr)
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

void SockOps::bind(int sockfd, InetAddr localAddr)
{
    struct sockaddr_in *addr = &localAddr.inetAddr();
    if (::bind(sockfd, sockaddr_cast(addr), sizeof(*addr)) < 0)
        logFatal("bind: %s", strerrno());
}

#define LISTENQ 1024

void SockOps::listen(int sockfd)
{
    if (::listen(sockfd, LISTENQ) < 0)
        logFatal("listen: %s", strerrno());
}

int SockOps::accept(int sockfd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int connfd = ::accept(sockfd, sockaddr_cast(&addr), &len);
    return connfd;
}

int SockOps::connect(int sockfd, InetAddr peerAddr)
{
    struct sockaddr_in *addr = &peerAddr.inetAddr();
    return ::connect(sockfd, sockaddr_cast(addr), sizeof(*addr));
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

    thread_local char sockops_ipaddr[32];
}

const char *SockOps::toHostIpAddr(const struct in_addr *addr)
{
    if (inet_ntop(AF_INET, addr, sockops_ipaddr, sizeof(sockops_ipaddr)) == nullptr)
        logError("inet_ntop: %s", strerrno());
    return sockops_ipaddr;
}

void SockOps::setKeepAlive(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
#ifdef _ANGEL_HAVE_TCP_KEEPALIVE
    int optval = TCP_KEEPALIVE;
#elif defined (_ANGEL_HAVE_SO_KEEPALIVE)
    int optval = SO_KEEPALIVE;
#endif
    if (::setsockopt(fd, SOL_SOCKET, optval, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> TCP_KEEPALIVE]: %s", strerrno());
    logInfo("%s TCP_KEEPALIVE", (on ? "enable" : "disable"));
}

void SockOps::setNoDelay(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> TCP_NODELAY]: %s", strerrno());
    logInfo("%s TCP_NODELAY", (on ? "enable" : "disable"));
}

void SockOps::setReuseAddr(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> SO_REUSEADDR]: %s", strerrno());
    logInfo("%s SO_REUSEADDR", (on ? "enable" : "disable"));
}

void SockOps::setReusePort(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        logError("[setsockopt -> SO_REUSEPORT]: %s", strerrno());
    logInfo("%s SO_REUSEPORT", (on ? "enable" : "disable"));
}
