#ifndef _ANGEL_POLL_H
#define _ANGEL_POLL_H

#include <sys/poll.h>
#include <vector>
#include <map>

#include "poller.h"
#include "noncopyable.h"

namespace angel {

class evloop;

class poll_base_t : public poller, noncopyable {
public:
    poll_base_t() { set_name("poll"); }
    ~poll_base_t() {  }
    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, event events) override;
    void change(int fd, event events) override;
    void remove(int fd, event events) override;
private:
    std::vector<struct pollfd> poll_fds;
    // <fd, index>, index for poll_fds
    std::map<int, int> indexs;
};
}

#endif // _ANGEL_POLL_H
