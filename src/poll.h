#ifndef _ANGEL_POLL_H
#define _ANGEL_POLL_H

#include <sys/poll.h>

#include <unordered_map>

#include "poller.h"

namespace angel {

class evloop;

class poll_base_t : public poller {
public:
    poll_base_t();
    ~poll_base_t();
    poll_base_t(const poll_base_t&) = delete;
    poll_base_t& operator=(const poll_base_t&) = delete;

    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    std::vector<struct pollfd> pollfds;
    // <fd, index>, index for pollfds
    std::unordered_map<int, int> indexs;
};
}

#endif // _ANGEL_POLL_H
