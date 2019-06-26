#ifndef _ANGEL_SOCKET_H
#define _ANGEL_SOCKET_H

#include "InetAddr.h"
#include "noncopyable.h"

namespace Angel {

class Socket : noncopyable {
public:
    explicit Socket(int fd);
    ~Socket();
    int fd() const { return _sockfd; }
    void setNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
private:
    const int _sockfd;
};
}

#endif // _ANGEL_SOCKET_H
