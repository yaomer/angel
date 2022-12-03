#ifndef __ANGEL_CONNECTOR_H
#define __ANGEL_CONNECTOR_H

#include <memory>
#include <functional>

#include <angel/inet_addr.h>
#include <angel/channel.h>

namespace angel {

class evloop;

class connector_t {
public:
    connector_t(evloop *, inet_addr);
    ~connector_t();

    connector_t(const connector_t&) = delete;
    connector_t& operator=(const connector_t&) = delete;

    const inet_addr& addr() const { return peer_addr; }
    void connect();

    const char *protocol = "tcp";
    bool keep_reconnect  = false;
    int retry_interval   = 3000; // 3s

    std::function<void(channel*)> onconnect;
    std::function<void()> onfail;
private:
    void connected();
    void shutdown();
    void check();
    void retry();

    evloop *loop;
    const inet_addr peer_addr;
    channel *connect_channel;
    size_t retry_timer_id;
};
}

#endif // __ANGEL_CONNECTOR_H
