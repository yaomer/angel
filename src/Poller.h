#ifndef _ANGEL_POLLER_H
#define _ANGEL_POLLER_H

#include <cinttypes>
#include <string>

namespace Angel {
class EventLoop;

class Poller {
public:
    virtual ~Poller() {  };
    virtual int wait(EventLoop *loop, int64_t timeout) = 0;
    virtual void add(int fd, int events) = 0;
    virtual void change(int fd, int events) = 0;
    // 为remove()添加events参数是出于Kqueue的需要
    virtual void remove(int fd, int events) = 0;

    void setName(const char *name) { _name = name; }
    const char *name() { return _name.c_str(); }
private:
    std::string _name;
};
}

#endif // _ANGEL_POLLER_H
