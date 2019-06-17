#ifndef _ANGEL_POLLER_H
#define _ANGEL_POLLER_H

#include <cinttypes>

namespace Angel {
class EventLoop;

class Poller {
public:
    virtual ~Poller() {  };
    virtual int wait(EventLoop *loop, int64_t timeout) = 0;
    virtual void add(int fd, int events) = 0;
    virtual void change(int fd, int events) = 0;
    virtual void remove(int fd) = 0;
};
}

#endif // _ANGEL_POLLER_H
