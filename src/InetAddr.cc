#include <arpa/inet.h>
#include "InetAddr.h"
#include "LogStream.h"

using namespace Angel;

InetAddr::InetAddr()
{
    // 只支持IPv4协议族
    _sockaddr.sin_family = AF_INET;
    // 服务端的默认设置
    _sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
}

void InetAddr::setPort(int port)
{
    _sockaddr.sin_port = htons(port);
}

void InetAddr::setAddr(const char *addr)
{
    if (inet_pton(AF_INET, addr, &_sockaddr.sin_addr) <= 0)
        LOG_FATAL << "inet_pton error: " << strerrno();
}
