#ifndef _ANGEL_INETADDR_H
#define _ANGEL_INETADDR_H

#include <netinet/in.h>

namespace Angel {

class InetAddr {
public:
    InetAddr();
    explicit InetAddr(int);
    InetAddr(int, const char *);
    InetAddr(struct sockaddr_in addr)
    {
        _sockaddr = addr;
    }
    InetAddr(const InetAddr& inetAddr)
    {
        _sockaddr = inetAddr._sockaddr;
    }
    InetAddr& operator=(const InetAddr& inetAddr)
    {
        _sockaddr = inetAddr._sockaddr;
        return *this;
    }
    struct sockaddr_in& inetAddr() { return _sockaddr; }
    int toIpPort();
    const char *toIpAddr();
private:
    struct sockaddr_in _sockaddr;
};
}

#endif // _ANGEL_INETADDR_H
