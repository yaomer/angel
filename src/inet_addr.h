#ifndef _ANGEL_INET_ADDR_H
#define _ANGEL_INET_ADDR_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include "sockops.h"
#include "util.h"
#include "logger.h"

namespace angel {

class inet_addr {
public:
    inet_addr() { clear(); }
    explicit inet_addr(int port)
    {
        fill("0.0.0.0", port);
    }
    inet_addr(const char *ip, int port)
    {
        fill(ip, port);
    }
    explicit inet_addr(const char *host)
    {
        std::string ip, port;
        const char *end = host + std::strlen(host);
        auto p = std::find(host, end, ':');
        if (p == end) log_fatal("%s error", host);
        ip.assign(host, p);
        port.assign(p + 1, end);
        fill(ip.c_str(), ::atoi(port.c_str()));
    }
    inet_addr(const struct sockaddr_in& addr)
    {
        sockaddr = addr;
    }
    inet_addr(const inet_addr& addr)
    {
        sockaddr = addr.sockaddr;
    }
    struct sockaddr_in& addr()
    {
        return sockaddr;
    }
    int to_host_port() const
    {
        return sockops::to_host_port(sockaddr.sin_port);
    }
    const char *to_host_ip() const
    {
        return sockops::to_host_ip(&sockaddr.sin_addr);
    }
    const char *to_host() const
    {
        return sockops::to_host(&sockaddr);
    }
private:
    void clear()
    {
        bzero(&sockaddr, sizeof(sockaddr));
    }
    void fill(const char *ip, int port)
    {
        clear();
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &sockaddr.sin_addr) <= 0)
            log_fatal("inet_pton: %s", util::strerrno());
    }

    struct sockaddr_in sockaddr;
};
}

#endif // _ANGEL_INET_ADDR_H
