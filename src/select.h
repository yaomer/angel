#ifndef _ANGEL_SELECT_H
#define _ANGEL_SELECT_H

#include <sys/select.h>
#include <vector>
#include <map>

#include "poller.h"
#include "noncopyable.h"

namespace angel {

class select_base_t : public poller, noncopyable {
public:
    select_base_t();
    ~select_base_t() {  };
    int wait(evloop *loop, int64_t timeout) override;
    void add(int fd, event events) override;
    void change(int fd, event events) override;
    void remove(int fd, event events) override;
private:
    static const size_t fds_init_size = 64;
    fd_set rdset;
    fd_set wrset;
    std::vector<int> fds;
    // <fd, index>, index for fds
    std::map<int, int> indexs;
    int maxfd;
};
}

#endif // _ANGEL_SELECT_H
