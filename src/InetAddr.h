#ifndef _ANGEL_INETADDR_H
#define _ANGEL_INETADDR_H

#include <netinet/in.h>

namespace Angel {

class InetAddr {
public:
    InetAddr();
    explicit InetAddr(int);
    InetAddr(int, const char *);
    struct sockaddr_in& inetAddr() { return _sockaddr; }
private:
    struct sockaddr_in _sockaddr;
};
}

#endif // _ANGEL_INETADDR_H
