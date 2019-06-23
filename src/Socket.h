#ifndef _ANGEL_SOCKET_H
#define _ANGEL_SOCKET_H

#include "InetAddr.h"
#include "noncopyable.h"

namespace Angel {

class Socket : noncopyable {
public:
    Socket(){  };
    explicit Socket(int fd) : _sockfd(fd) {  };
    ~Socket();
    int fd() const { return _sockfd; }
    void socket();
    void bind(InetAddr&);
    void listen();
    int accept();
    int connect(InetAddr&);
    void setNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
private:
    int _sockfd;
};
}

#endif // _ANGEL_SOCKET_H
