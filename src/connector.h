#ifndef _ANGEL_CONNECTOR_H
#define _ANGEL_CONNECTOR_H

#include <memory>
#include <functional>

#include "inet_addr.h"
#include "channel.h"

namespace angel {

class evloop;

class connector_t {
public:
    typedef std::function<void(int)> new_connection_handler_t;
    connector_t(evloop *, inet_addr,
                const new_connection_handler_t,
                int64_t retry_interval_ms,
                std::string_view protocol);
    ~connector_t();
    connector_t(const connector_t&) = delete;
    connector_t& operator=(const connector_t&) = delete;

    void connect();
    bool is_connected() { return has_connected; }
    int connfd() const { return connect_channel->fd(); }
    inet_addr& addr() { return peer_addr; }
private:
    void connecting();
    void connected();
    void check();
    void retry();

    evloop *loop;
    inet_addr peer_addr;
    std::shared_ptr<channel> connect_channel;
    new_connection_handler_t new_connection_handler;
    bool has_connected;
    size_t retry_timer_id;
    bool wait_retry;
    int sockfd;
    int retry_interval;
    std::string protocol;
};
}

#endif // _ANGEL_CONNECTOR_H
