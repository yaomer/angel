#ifndef _ANGEL_SOCKET_H
#define _ANGEL_SOCKET_H

#include "InetAddr.h"
#include "Noncopyable.h"

namespace Angel {

class Socket : Noncopyable {
public:
    ~Socket();
    int fd() const { return _sockfd; }
    void setFd(int fd) { _sockfd = fd; }
    void socket();
    void bind(InetAddr&);
    void listen();
    int accept();
    int connect(InetAddr&);
    void setNonBlock();
    void setNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    int socketError();
    static void socketpair(int fd[]);
    static void setNonBlock(int fd);
private:
    int _sockfd = -1;
};
}

#endif // _ANGEL_SOCKET_H
