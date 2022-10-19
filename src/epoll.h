#ifndef _ANGEL_EPOLL_H
#define _ANGEL_EPOLL_H

#include <sys/epoll.h>

#include "poller.h"

namespace angel {

class evloop;

class epoll_base_t : public poller {
public:
    epoll_base_t();
    ~epoll_base_t();
    epoll_base_t(const epoll_base_t&) = delete;
    epoll_base_t& operator=(const epoll_base_t&) = delete;

    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    int epfd;
    int added_fds = 0;
    std::vector<struct epoll_event> evlist;
    std::vector<int> evmap;
};

}

#endif // _ANGEL_EPOLL_H
