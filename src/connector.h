#ifndef _ANGEL_CONNECTOR_H
#define _ANGEL_CONNECTOR_H

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

    void connect();
    bool is_connected() { return has_connected; }
    int connfd() const { return connect_channel->fd(); }
    inet_addr& addr() { return peer_addr; }

    typedef std::function<void(int)> new_connection_handler_t;
    new_connection_handler_t new_connection_handler;
    std::string protocol = "tcp";
    int retry_interval = 1000; // 1s
private:
    void connecting();
    void connected();
    void check();
    void retry();

    evloop *loop;
    inet_addr peer_addr;
    std::shared_ptr<channel> connect_channel;
    bool has_connected;
    size_t retry_timer_id;
    bool wait_retry;
    int sockfd;
};
}

#endif // _ANGEL_CONNECTOR_H
