#include <angel/sockops.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string.h>

#include <angel/util.h>
#include <angel/logger.h>
#include "config.h"

namespace angel {

using namespace util;

namespace sockops {

static inline struct sockaddr *sockaddr_cast(struct sockaddr_in *addr)
{
    return reinterpret_cast<struct sockaddr *>(addr);
}

int socket(std::string_view protocol)
{
    int sockfd = -1;
    if (protocol == "tcp") {
        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    } else if (protocol == "udp") {
        sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        log_fatal("protocol should be set (tcp or udp)");
    }
    if (sockfd < 0)
        log_fatal("socket: %s", strerrno());
    return sockfd;
}

void bind(int sockfd, struct sockaddr_in *localAddr)
{
    if (::bind(sockfd, sockaddr_cast(localAddr), sizeof(*localAddr)) < 0)
        log_fatal("bind: %s", strerrno());
}

#define LISTENQ 1024

void listen(int sockfd)
{
    if (::listen(sockfd, LISTENQ) < 0)
        log_fatal("listen: %s", strerrno());
}

int accept(int sockfd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int connfd = ::accept(sockfd, sockaddr_cast(&addr), &len);
    return connfd;
}

int connect(int sockfd, struct sockaddr_in *peerAddr)
{
    return ::connect(sockfd, sockaddr_cast(peerAddr), sizeof(*peerAddr));
}

void set_nonblock(int sockfd)
{
    int oflag = ::fcntl(sockfd, F_GETFL, 0);
    ::fcntl(sockfd, F_SETFL, oflag | O_NONBLOCK);
}

void socketpair(int sockfd[])
{
    if (::socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd) < 0)
        log_fatal("socketpair: %s", strerrno());
}

struct sockaddr_in get_local_addr(int sockfd)
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
    if (::getsockname(sockfd, sockaddr_cast(&addr), &len) < 0)
        log_error("getsockname: %s", strerrno());
    return addr;
}

struct sockaddr_in get_peer_addr(int sockfd)
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
    if (::getpeername(sockfd, sockaddr_cast(&addr), &len) < 0)
        log_error("getpeername: %s", strerrno());
    return addr;
}

int get_socket_error(int sockfd)
{
    socklen_t err;
    socklen_t len = sizeof(err);
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        log_error("[getsockopt -> SO_ERROR]: %s", strerrno());
    return static_cast<int>(err);
}

int to_host_port(in_port_t port)
{
    return ntohs(port);
}

const char *to_host_ip(const struct in_addr *addr)
{
    static thread_local char host_ip[32];
    if (inet_ntop(AF_INET, addr, host_ip, sizeof(host_ip)) == nullptr) {
        log_error("inet_ntop: %s", strerrno());
        return nullptr;
    }
    return host_ip;
}

const char *to_host(const struct sockaddr_in *addr)
{
    static thread_local char host[48];
    int port = to_host_port(addr->sin_port);
    const char *ip = to_host_ip(&addr->sin_addr);
    snprintf(host, sizeof(host), "%s:%d", ip, port);
    return host;
}

const char *get_host_name()
{
    static thread_local char hostname[256];
    if (::gethostname(hostname, sizeof(hostname)) < 0) {
        log_error("gethostname: %s", strerrno());
        return nullptr;
    }
    return hostname;
}

#if defined (__linux__)
#include <sys/sendfile.h>
#include <sys/stat.h>
#endif

int send_file(int fd, int sockfd)
{
#if defined (__APPLE__)
    off_t len = 0;
    int ret = sendfile(fd, sockfd, 0, &len, nullptr, 0);
    return ret;
#elif defined (__linux__)
    struct stat st;
    fstat(fd, &st);
    int ret = sendfile(sockfd, fd, nullptr, st.st_size);
    return ret;
#endif
    errno = ENOTSUP;
    return -1;
}

void set_keepalive(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
#if defined (ANGEL_HAVE_TCP_KEEPALIVE)
    int optval = TCP_KEEPALIVE;
#elif defined (ANGEL_HAVE_SO_KEEPALIVE)
    int optval = SO_KEEPALIVE;
#endif
    if (::setsockopt(fd, SOL_SOCKET, optval, &opt, sizeof(opt)) < 0)
        log_fatal("[setsockopt -> TCP_KEEPALIVE]: %s", strerrno());
    log_debug("%s TCP_KEEPALIVE", (on ? "enable" : "disable"));
}

void set_nodelay(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        log_fatal("[setsockopt -> TCP_NODELAY]: %s", strerrno());
    log_debug("%s TCP_NODELAY", (on ? "enable" : "disable"));
}

void set_reuseaddr(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        log_fatal("[setsockopt -> SO_REUSEADDR]: %s", strerrno());
    log_debug("%s SO_REUSEADDR", (on ? "enable" : "disable"));
}

void set_reuseport(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        log_fatal("[setsockopt -> SO_REUSEPORT]: %s", strerrno());
    log_debug("%s SO_REUSEPORT", (on ? "enable" : "disable"));
}

}
}
