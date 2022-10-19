#ifndef _ANGEL_SELECT_H
#define _ANGEL_SELECT_H

#include <sys/select.h>

#include <unordered_set>

#include "poller.h"

namespace angel {

class select_base_t : public poller {
public:
    select_base_t();
    ~select_base_t();
    select_base_t(const select_base_t&) = delete;
    select_base_t& operator=(const select_base_t&) = delete;

    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    fd_set rdset, _rdset;
    fd_set wrset, _wrset;
    std::unordered_set<int> fds;
    int maxfd;
};
}

#endif // _ANGEL_SELECT_H
