#ifndef _ANGEL_KQUEUE_H
#define _ANGEL_KQUEUE_H

#include <sys/event.h>

#include "poller.h"

namespace angel {

class evloop;

class kqueue_base_t : public poller {
public:
    kqueue_base_t();
    ~kqueue_base_t();
    kqueue_base_t(const kqueue_base_t&) = delete;
    kqueue_base_t& operator=(const kqueue_base_t&) = delete;

    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    int kqfd;
    int added_fds = 0;
    std::vector<struct kevent> evlist;
    std::vector<int> evmap;
};
}

#endif // _ANGEL_KQUEUE_H
