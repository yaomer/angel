#ifndef _ANGEL_POLLER_H
#define _ANGEL_POLLER_H

#include <string>
#include <vector>

namespace angel {

class evloop;

// Encapsulate the underlying I/O multiplexing to provide
// a unified interface for the upper layer.
class poller {
public:
    virtual ~poller() {  };
    virtual int wait(evloop *loop, int64_t timeout) = 0;
    // Add interesting events to fd.
    // If fd does not exist, it will be registered in poller.
    virtual void add(int fd, int events) = 0;
    // Remove events from monitered events for fd.
    // If fd has no events of interest, remove itself from poller.
    virtual void remove(int fd, int events) = 0;

    void set_name(const char *name) { poller_name = name; }
    const char *name() { return poller_name.c_str(); }

    static const int EVLIST_INIT_SIZE = 64;
private:
    std::string poller_name;
};

template <typename T>
inline void resize_if(int fd, std::vector<T>& vec)
{
    if (fd >= vec.size())
        vec.resize(vec.size() * 2);
}

}

#endif // _ANGEL_POLLER_H
