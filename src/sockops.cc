#include <angel/sockops.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>

#include <angel/util.h>
#include <angel/logger.h>

namespace angel {

using namespace util;

namespace sockops {

static struct sockaddr *sockaddr_cast(struct sockaddr_in *addr)
{
    return reinterpret_cast<struct sockaddr *>(addr);
}

static const struct sockaddr *sockaddr_const_cast(const struct sockaddr_in *addr)
{
    return reinterpret_cast<const struct sockaddr *>(addr);
}

// socket() creates an endpoint for communication and returns
// a file descriptor that refers to that endpoint.
int socket(const char *protocol)
{
    int sockfd = -1;
    if (strcmp(protocol, "tcp") == 0) {
        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    } else if (strcmp(protocol, "udp") == 0) {
        sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        log_fatal("protocol should be set (tcp or udp)");
    }
    if (sockfd < 0)
        log_fatal("socket: %s", strerrno());
    return sockfd;
}

// bind() assigns the address specified by local_addr to the socket
// referred to by the sockfd.
void bind(int sockfd, const struct sockaddr_in *local_addr)
{
    if (::bind(sockfd, sockaddr_const_cast(local_addr), sizeof(*local_addr)) < 0)
        log_fatal("bind: %s", strerrno());
}

// listen() marks the socket referred to by sockfd as a passive socket,
// that is, as a socket that will be used to accept incoming connection
// requests using accept(2).
void listen(int sockfd)
{
    // The backlog parameter defines the maximum length for
    // the queue of pending connections.
    //
    // backlog = min(somaxconn, backlog)
    //
    int backlog = 1024;
    if (::listen(sockfd, backlog) < 0)
        log_fatal("listen: %s", strerrno());
}

// accept() extracts the first connection request on the queue of
// pending connections for the listening socket.
int accept(int sockfd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int connfd = ::accept(sockfd, sockaddr_cast(&addr), &len);
    return connfd;
}

// For SOCK_STREAM socket, connect() attempts to make a connection to peer socket.
// For SOCK_DGRAM socket, connect() specifies the peer socket is that to which
// datagrams are to be sent.
//
// Generally, stream sockets may successfully connect() only once; datagram sockets
// may use connect() multiple times to change their association.
//
// For datagram sockets that need continuous communication, calling connect()
// may be a better choice, because it avoids dynamically registering and
// deleting peer socket when sendto(2) is called every time.
//
int connect(int sockfd, const struct sockaddr_in *peer_addr)
{
    return ::connect(sockfd, sockaddr_const_cast(peer_addr), sizeof(*peer_addr));
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

void set_reuseaddr(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        log_error("setsockopt(SO_REUSEADDR): %s", strerrno());
}

void set_reuseport(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        log_error("setsockopt(SO_REUSEPORT): %s", strerrno());
}

void set_nodelay(int fd, bool on)
{
    socklen_t opt = on ? 1 : 0;
#if defined (__APPLE__)
    if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        log_error("setsockopt(TCP_NODELAY): %s", strerrno());
#elif defined (__linux__)
    if (::setsockopt(fd, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        log_error("setsockopt(TCP_NODELAY): %s", strerrno());
#endif
}

void set_keepalive(int fd, bool on)
{
    socklen_t optval = on ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0)
        log_error("setsockopt(SO_KEEPALIVE): %s", strerrno());
}

void set_keepalive_idle(int fd, int idle)
{
    if (idle <= 0) return;
#if defined (__APPLE__)
    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle)) < 0)
        log_error("setsockopt(TCP_KEEPALIVE): %s", strerrno());
#elif defined (__linux__)
    if (::setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0)
        log_error("setsockopt(TCP_KEEPIDLE): %s", strerrno());
#endif
}

void set_keepalive_intvl(int fd, int intvl)
{
    if (intvl <= 0) return;
#if defined (__APPLE__)
    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) < 0)
        log_error("setsockopt(TCP_KEEPINTVL): %s", strerrno());
#elif defined (__linux__)
    if (::setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) < 0)
        log_error("setsockopt(TCP_KEEPINTVL): %s", strerrno());
#endif
}

void set_keepalive_probes(int fd, int probes)
{
    if (probes <= 0) return;
#if defined (__APPLE__)
    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes)) < 0)
        log_error("setsockopt(TCP_KEEPCNT): %s", strerrno());
#elif defined (__linux__)
    if (::setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &probes, sizeof(probes)) < 0)
        log_error("setsockopt(TCP_KEEPCNT): %s", strerrno());
#endif
}

#if defined (__linux__)
#include <sys/sendfile.h>
#endif

ssize_t sendfile(int fd, int sockfd, off_t offset, off_t count)
{
#if defined (__APPLE__)
    int rc = ::sendfile(fd, sockfd, offset, &count, nullptr, 0);
    // If errno == EAGAIN or EINTR,
    // the number of bytes successfully sent will be returned in *len.
    if (rc == -1 && errno != EAGAIN && errno != EINTR) return -1;
    return count;
#elif defined (__linux__)
    // If the transfer was successful, the number of bytes written to
    // sockfd is returned. Note that a successful call to sendfile()
    // may write fewer bytes than requested.
    return ::sendfile(sockfd, fd, &offset, count);
#endif
}

}
}
