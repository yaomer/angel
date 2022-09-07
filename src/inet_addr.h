#ifndef _ANGEL_INET_ADDR_H
#define _ANGEL_INET_ADDR_H

#include <string>

#include <netinet/in.h>

namespace angel {

class inet_addr {
public:
    inet_addr();
    explicit inet_addr(int port);
    inet_addr(const std::string& ip, int port);
    // host ==> ip:port
    explicit inet_addr(const std::string& host);
    inet_addr(const struct sockaddr_in& addr);
    inet_addr(const inet_addr& addr);

    struct sockaddr_in& addr();
    int to_host_port() const;
    const char *to_host_ip() const;
    const char *to_host() const;
private:
    struct sockaddr_in sockaddr;
};
}

#endif // _ANGEL_INET_ADDR_H
