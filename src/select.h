#ifndef _ANGEL_SELECT_H
#define _ANGEL_SELECT_H

#include <sys/select.h>
#include <vector>
#include <map>

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
    void change(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    fd_set rdset;
    fd_set wrset;
    std::vector<int> fds;
    // <fd, index>, index for fds
    std::map<int, int> indexs;
    int maxfd;
};
}

#endif // _ANGEL_SELECT_H
