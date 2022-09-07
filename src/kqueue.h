#ifndef _ANGEL_KQUEUE_H
#define _ANGEL_KQUEUE_H

#include <sys/event.h>
#include <vector>
#include <map>

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
    void change(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    int kqfd;
    size_t added_fds;
    std::vector<struct kevent> evlist;
};
}

#endif // _ANGEL_KQUEUE_H
