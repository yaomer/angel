#ifndef _ANGEL_EPOLL_H
#define _ANGEL_EPOLL_H

#include <sys/epoll.h>
#include <vector>

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
    void change(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    int epfd;
    size_t added_fds;
    std::vector<struct epoll_event> evlist;
};

}

#endif // _ANGEL_EPOLL_H
