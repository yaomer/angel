#include <arpa/inet.h>

#include "inet_addr.h"
#include "sockops.h"
#include "logger.h"
#include "util.h"

namespace angel {

inet_addr::inet_addr() : sockaddr{}
{
}

static void fill(struct sockaddr_in& sockaddr, const std::string& ip, int port)
{
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &sockaddr.sin_addr) <= 0)
        log_fatal("inet_pton: %s", util::strerrno());
}


inet_addr::inet_addr(int port)
{
    fill(sockaddr, "0.0.0.0", port);
}

inet_addr::inet_addr(const std::string& ip, int port)
{
    fill(sockaddr, ip, port);
}

inet_addr::inet_addr(const std::string& host)
{
    std::string ip, port;
    auto p = std::find(host.begin(), host.end(), ':');
    if (p == host.end()) log_fatal("%s error", host.c_str());
    ip.assign(host.begin(), p);
    port.assign(p + 1, host.end());
    fill(sockaddr, ip, atoi(port.c_str()));
}

inet_addr::inet_addr(const struct sockaddr_in& addr)
    : sockaddr(addr)
{
}

inet_addr::inet_addr(const inet_addr& addr)
    : sockaddr(addr.sockaddr)
{
}

struct sockaddr_in& inet_addr::addr()
{
    return sockaddr;
}

int inet_addr::to_host_port() const
{
    return sockops::to_host_port(sockaddr.sin_port);
}

const char *inet_addr::to_host_ip() const
{
    return sockops::to_host_ip(&sockaddr.sin_addr);
}

const char *inet_addr::to_host() const
{
    return sockops::to_host(&sockaddr);
}

}