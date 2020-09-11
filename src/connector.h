#ifndef _ANGEL_CONNECTOR_H
#define _ANGEL_CONNECTOR_H

#include <memory>
#include <functional>

#include "inet_addr.h"
#include "channel.h"
#include "noncopyable.h"

namespace angel {

class evloop;

class connector_t : noncopyable {
public:
    typedef std::function<void(int)> new_connection_handler_t;
    connector_t(evloop *, inet_addr);
    ~connector_t() {  };
    void connect();
    bool is_connected() { return has_connected; }
    int connfd() const { return connect_channel->fd(); }
    inet_addr& addr() { return peer_addr; }
    void set_new_connection_handler(const new_connection_handler_t handler)
    { new_connection_handler = std::move(handler); }

private:
    static const int retry_interval = 3;

    void connecting(int fd);
    void connected(int fd);
    void check(int fd);
    void retry(int fd);

    evloop *loop;
    inet_addr peer_addr;
    std::shared_ptr<channel> connect_channel;
    new_connection_handler_t new_connection_handler;
    bool has_connected;
    bool wait_retry;
};
}

#endif // _ANGEL_CONNECTOR_H
