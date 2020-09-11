#ifndef _ANGEL_KQUEUE_H
#define _ANGEL_KQUEUE_H

#include <sys/event.h>
#include <vector>
#include <map>

#include "poller.h"
#include "noncopyable.h"

namespace angel {

class evloop;

class kqueue_base_t : public poller, noncopyable {
public:
    kqueue_base_t();
    ~kqueue_base_t();
    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, event events) override;
    void change(int fd, event events) override;
    void remove(int fd, event events) override;
private:
    static const size_t evlist_init_size = 64;
    int kqfd;
    size_t added_fds;
    std::vector<struct kevent> evlist;
};
}

#endif // _ANGEL_KQUEUE_H
