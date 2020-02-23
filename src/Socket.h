#ifndef _ANGEL_SOCKET_H
#define _ANGEL_SOCKET_H

#include <unistd.h>

#include "noncopyable.h"

namespace Angel {

class Socket : noncopyable {
public:
    explicit Socket(int fd) : _sockfd(fd)
    {
    }
    ~Socket() { close(_sockfd); };
    int fd() const { return _sockfd; }
private:
    const int _sockfd;
};
}

#endif // _ANGEL_SOCKET_H
