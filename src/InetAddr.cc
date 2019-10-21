#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

#include "InetAddr.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;

InetAddr::InetAddr()
{
    bzero(&_sockaddr, sizeof(_sockaddr));
}

InetAddr::InetAddr(int port)
{
    bzero(&_sockaddr, sizeof(_sockaddr));
    _sockaddr.sin_family = AF_INET;
    _sockaddr.sin_port = htons(port);
    _sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
}

InetAddr::InetAddr(int port, const char *addr)
{
    bzero(&_sockaddr, sizeof(_sockaddr));
    _sockaddr.sin_family = AF_INET;
    _sockaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &_sockaddr.sin_addr) <= 0)
        logFatal("inet_pton: %s", strerrno());
}

int InetAddr::toIpPort()
{
    return SockOps::toHostIpPort(_sockaddr.sin_port);
}

const char *InetAddr::toIpAddr()
{
    return SockOps::toHostIpAddr(&_sockaddr.sin_addr);
}
