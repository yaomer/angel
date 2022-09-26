#ifndef _ANGEL_POLLER_H
#define _ANGEL_POLLER_H

#include <string>

namespace angel {

class evloop;

// Encapsulate the underlying I/O multiplexing to provide
// a unified interface for the upper layer.
class poller {
public:
    virtual ~poller() {  };
    virtual int wait(evloop *loop, int64_t timeout) = 0;
    virtual void add(int fd, int events) = 0;
    virtual void change(int fd, int events) = 0;
    // Add the events parameter to remove() is for the needs of kqueue
    virtual void remove(int fd, int events) = 0;

    void set_name(const char *name) { poller_name = name; }
    const char *name() { return poller_name.c_str(); }
private:
    std::string poller_name;
};
}

#endif // _ANGEL_POLLER_H
