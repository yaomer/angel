#ifndef _ANGEL_SOCKET_H
#define _ANGEL_SOCKET_H

#include <unistd.h>

#include "noncopyable.h"

namespace angel {

class socket : noncopyable {
public:
    explicit socket(int fd) : sockfd(fd)
    {
    }
    ~socket() { close(sockfd); };
    int fd() const { return sockfd; }
private:
    const int sockfd;
};
}

#endif // _ANGEL_SOCKET_H
