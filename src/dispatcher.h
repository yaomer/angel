#ifndef __ANGEL_DISPATCHER_H
#define __ANGEL_DISPATCHER_H

#include <string>
#include <vector>

namespace angel {

class evloop;

// Encapsulate the underlying I/O multiplexing to provide
// a unified interface for the upper layer.
class dispatcher {
public:
    virtual ~dispatcher() {  };
    virtual int wait(evloop *loop, int64_t timeout) = 0;
    // Add interesting events to fd.
    // If fd does not exist, it will be registered in dispatcher.
    virtual void add(int fd, int events) = 0;
    // Remove events from monitered events for fd.
    // If fd has no events of interest, remove itself from dispatcher.
    virtual void remove(int fd, int events) = 0;

    void set_name(const char *name) { dispatcher_name = name; }
    const char *name() { return dispatcher_name.c_str(); }

    static const int EVLIST_INIT_SIZE = 64;
private:
    std::string dispatcher_name;
};

template <typename T>
inline void resize_if(int fd, std::vector<T>& vec)
{
    if (fd >= vec.size())
        vec.resize(vec.size() * 2);
}

}

#endif // __ANGEL_DISPATCHER_H
