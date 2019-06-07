#ifndef _ANGEL_INETADDR_H
#define _ANGEL_INETADDR_H

#include <netinet/in.h>

namespace Angel {

class InetAddr {
public:
    InetAddr();
    struct sockaddr_in& inetAddr() { return _sockaddr; }
    void setPort(int port);
    void setAddr(const char *addr);
private:
    struct sockaddr_in _sockaddr;
};
}

#endif // _ANGEL_INETADDR_H
