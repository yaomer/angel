#ifndef _ANGEL_EPOLL_H
#define _ANGEL_EPOLL_H

#include <sys/epoll.h>
#include <vector>

#include "Poller.h"
#include "noncopyable.h"

namespace Angel {

class EventLoop;

class Epoll : public Poller, noncopyable {
public:
    Epoll();
    ~Epoll();
    int wait(EventLoop *loop, int64_t timeout) override;
    void add(int fd, int events) override;
    void change(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    static const size_t evlist_init_size = 64;
    int _epfd;
    size_t _addFds;
    std::vector<struct epoll_event> _evList;
    size_t _evListSize;
};

}

#endif // _ANGEL_EPOLL_H
