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
    int wait(EventLoop *loop, int64_t timeout);
    void add(int fd, int events);
    void change(int fd, int events);
    void remove(int fd, int events);
    const int _INIT_EVLIST_SIZE = 64;
private:
    void evset(struct epoll_event& ev, int fd, int events);
    int evret(int events);

    int _epfd;
    size_t _addFds;
    std::vector<struct epoll_event> _evList;
    size_t _evListSize;
};

}

#endif // _ANGEL_EPOLL_H
