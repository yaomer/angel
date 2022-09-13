#ifndef _ANGEL_SOCKET_H
#define _ANGEL_SOCKET_H

#include <unistd.h>

namespace angel {

class socket {
public:
    explicit socket(int fd) : sockfd(fd) {  }
    ~socket() { close(sockfd); };
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;
    int fd() const { return sockfd; }
private:
    const int sockfd;
};
}

#endif // _ANGEL_SOCKET_H
