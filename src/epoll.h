#ifndef _ANGEL_EPOLL_H
#define _ANGEL_EPOLL_H

#include <sys/epoll.h>
#include <vector>

#include "poller.h"
#include "noncopyable.h"

namespace angel {

class evloop;

class epoll_base_t : public poller, noncopyable {
public:
    epoll_base_t();
    ~epoll_base_t();
    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, event events) override;
    void change(int fd, event events) override;
    void remove(int fd, event events) override;
private:
    static const size_t evlist_init_size = 64;
    int epfd;
    size_t added_fds;
    std::vector<struct epoll_event> evlist;
};

}

#endif // _ANGEL_EPOLL_H
