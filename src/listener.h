#ifndef __ANGEL_LISTENER_H
#define __ANGEL_LISTENER_H

#include <memory>
#include <functional>

#include <angel/inet_addr.h>
#include <angel/channel.h>

namespace angel {

class evloop;

class listener_t {
public:
    listener_t(evloop *, inet_addr);
    ~listener_t();

    listener_t(const listener_t&) = delete;
    listener_t& operator=(const listener_t&) = delete;

    const inet_addr& addr() const { return listen_addr; }
    void listen();

    // Set options on listen socket fd.
    bool nodelay   = false;
    bool keepalive = true;
    int keepalive_idle   = 0; // 0 will be ignored
    int keepalive_intvl  = 0; // 0 will be ignored
    int keepalive_probes = 0; // 0 will be ignored

    // Called (in loop thread) after accept(2) returns successfully.
    // The accepted fd will be passed to onaccept(fd).
    std::function<void(int)> onaccept;
private:
    void handle_accept();

    evloop *loop;
    channel *listen_channel;
    const inet_addr listen_addr;
    int idle_fd;
};
}

#endif // __ANGEL_LISTENER_H
