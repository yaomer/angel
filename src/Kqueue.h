#ifndef _ANGEL_KQUEUE_H
#define _ANGEL_KQUEUE_H

#include <sys/event.h>
#include <vector>
#include <map>
#include "Poller.h"
#include "noncopyable.h"

namespace Angel {

class EventLoop;

class Kqueue : public Poller, noncopyable {
public:
    Kqueue();
    ~Kqueue();
    int wait(EventLoop *loop, int64_t timeout);
    void add(int fd, int events);
    void change(int fd, int events);
    void remove(int fd);
    const int _INIT_EVLIST_SIZE = 64;
private:
    int evret(struct kevent&);

    int _kqfd;
    int _addFds = 0;
    std::vector<struct kevent> _evlist;
    size_t _evlistSize;
};
}

#endif // _ANGEL_KQUEUE_H
