#include <arpa/inet.h>
#include <sys/socket.h>
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
        LOG_FATAL << "inet_pton error: " << strerrno();
}

InetAddr::InetAddr(struct sockaddr_in addr)
{
    _sockaddr = std::move(addr);
}

int InetAddr::toIpPort()
{
    return SockOps::toHostIpPort(_sockaddr.sin_port);
}

const char *InetAddr::toIpAddr()
{
    return SockOps::toHostIpAddr(&_sockaddr.sin_addr);
}
