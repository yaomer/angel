#ifndef _ANGEL_SOCKOPS_H
#define _ANGEL_SOCKOPS_H

#include <arpa/inet.h>

#include "InetAddr.h"

namespace Angel {

namespace SockOps {

int socket();
void bind(int sockfd, InetAddr localAddr);
void listen(int sockfd);
int accept(int sockfd);
int connect(int sockfd, InetAddr peerAddr);

void setnonblock(int sockfd);
void socketpair(int sockfd[]);

struct sockaddr_in getLocalAddr(int sockfd);
struct sockaddr_in getPeerAddr(int sockfd);
int getSocketError(int sockfd);

int toHostIpPort(in_port_t port);
const char *toHostIpAddr(const struct in_addr *addr);

void setKeepAlive(int fd, bool on);
void setNoDelay(int fd, bool on);
void setReuseAddr(int fd, bool on);
void setReusePort(int fd, bool on);

}
}

#endif // _ANGEL_SOCKOPS_H
