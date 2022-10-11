#ifndef _ANGEL_LISTENER_H
#define _ANGEL_LISTENER_H

#include <memory>
#include <functional>

#include <angel/inet_addr.h>
#include <angel/socket.h>
#include <angel/buffer.h>
#include <angel/channel.h>

namespace angel {

class evloop;

class listener_t {
public:
    typedef std::function<void(int)> new_connection_handler_t;
    listener_t(evloop *, inet_addr, const new_connection_handler_t);
    ~listener_t();

    listener_t(const listener_t&) = delete;
    listener_t& operator=(const listener_t&) = delete;

    void listen();
    int fd() const { return listen_socket.fd(); }
    inet_addr& addr() { return listen_addr; }

    // Set options on listen socket
    bool nodelay   = false;
    bool keepalive = true;
    int keepalive_idle   = 0; // 0 will be ignored
    int keepalive_intvl  = 0; // 0 will be ignored
    int keepalive_probes = 0; // 0 will be ignored
private:
    void handle_accept();

    evloop *loop;
    std::shared_ptr<channel> listen_channel;
    socket listen_socket;
    inet_addr listen_addr;
    new_connection_handler_t new_connection_handler;
    int idle_fd;
};
}

#endif // _ANGEL_LISTENER_H
