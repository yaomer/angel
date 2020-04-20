#ifndef _ANGEL_SELECT_H
#define _ANGEL_SELECT_H

#include <sys/select.h>
#include <vector>
#include <map>

#include "Poller.h"
#include "noncopyable.h"

namespace Angel {

class Select : public Poller, noncopyable {
public:
    Select();
    ~Select() {  };
    int wait(EventLoop *loop, int64_t timeout) override;
    void add(int fd, int events) override;
    void change(int fd, int events) override;
    void remove(int fd, int events) override;
private:
    static const size_t fds_init_size = 64;
    fd_set _rdset;
    fd_set _wrset;
    std::vector<int> _fds;
    // <fd, index>, index for _fds
    std::map<int, int> _indexs;
    int _maxFd;
};
}

#endif // _ANGEL_SELECT_H
