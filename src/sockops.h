#ifndef _ANGEL_SOCKOPS_H
#define _ANGEL_SOCKOPS_H

#include <arpa/inet.h>

namespace angel {

namespace sockops {

int socket();
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
