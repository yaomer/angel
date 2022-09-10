#ifndef _ANGEL_SOCKOPS_H
#define _ANGEL_SOCKOPS_H

#include <arpa/inet.h>

#include <string_view>

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

int socket(std::string_view protocol = "tcp");
void bind(int sockfd, struct sockaddr_in *addr);
void listen(int sockfd);
int accept(int sockfd);
int connect(int sockfd, struct sockaddr_in *addr);

void set_nonblock(int sockfd);
void socketpair(int sockfd[]);

struct sockaddr_in get_local_addr(int sockfd);
struct sockaddr_in get_peer_addr(int sockfd);
int get_socket_error(int sockfd);

int to_host_port(in_port_t port);
const char *to_host_ip(const struct in_addr *addr);
const char *to_host(const struct sockaddr_in *addr);

int send_file(int fd, int sockfd);

void set_keepalive(int fd, bool on);
void set_nodelay(int fd, bool on);
void set_reuseaddr(int fd, bool on);
void set_reuseport(int fd, bool on);

}
}

#endif // _ANGEL_SOCKOPS_H
