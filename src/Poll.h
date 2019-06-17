#ifndef _ANGEL_POLL_H
#define _ANGEL_POLL_H

#include <sys/poll.h>
#include <vector>
#include <map>
#include "Poller.h"
#include "Noncopyable.h"

namespace Angel {

class EventLoop;

class Poll : public Poller, Noncopyable {
public:
    int wait(EventLoop *loop, int64_t timeout);
    void add(int fd, int events);
    void change(int fd, int events);
    void remove(int fd);
private:
    void evset(struct pollfd& ev, int fd, int events);
    int evret(int events);

    std::vector<struct pollfd> _pollfds;
    // <fd, index>, index for _pollfds
    std::map<int, int> _indexs;
};
}

#endif // _ANGEL_POLL_H
