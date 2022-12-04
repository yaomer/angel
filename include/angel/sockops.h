#ifndef __ANGEL_SOCKOPS_H
#define __ANGEL_SOCKOPS_H

#include <unistd.h>
#include <arpa/inet.h>

namespace angel {
namespace sockops {

static inline uint64_t hton64(uint64_t host)
{
#if __LITTLE_ENDIAN__
    return ((uint64_t)htonl(host & 0xffffffff) << 32) | htonl(host >> 32);
#else
    return host;
#endif
}

static inline uint64_t ntoh64(uint64_t net)
{
#if __LITTLE_ENDIAN__
    return ((uint64_t)ntohl(net & 0xffffffff) << 32) | ntohl(net >> 32);
#else
    return net;
#endif
}

int socket(const char *protocol = "tcp");
void bind(int sockfd, const struct sockaddr_in *addr);
void listen(int sockfd);
int accept(int sockfd);
int connect(int sockfd, const struct sockaddr_in *addr);

void set_nonblock(int sockfd);
void socketpair(int sockfd[]);

struct sockaddr_in get_local_addr(int sockfd);
struct sockaddr_in get_peer_addr(int sockfd);
int get_socket_error(int sockfd);
const char *get_host_name();

int to_host_port(in_port_t port);
const char *to_host_ip(const struct in_addr *addr);
const char *to_host(const struct sockaddr_in *addr);

void set_reuseaddr(int fd, bool on);
void set_reuseport(int fd, bool on);
void set_nodelay(int fd, bool on);

void set_keepalive(int fd, bool on);
// Setting is effective only after SO_KEEPALIVE is enabled.
// idle time (secs)
void set_keepalive_idle(int fd, int idle);
// interval between keepalives
void set_keepalive_intvl(int fd, int intvl);
// probes of keepalives before close
void set_keepalive_probes(int fd, int probes);

// sockfd should be set nonblock
//
// If the transfer was successful, the number of bytes written to sockfd is returned.
// Note that a successful call to sendfile() may write fewer bytes than requested;
// the caller should be prepared to retry the call if there were unsent bytes.
//
// On error, -1 is returned, and errno is set to indicate the error.
//
ssize_t sendfile(int fd, int sockfd, off_t offset, off_t count);

}
}

#endif // __ANGEL_SOCKOPS_H
