#ifndef _ANGEL_POLLER_H
#define _ANGEL_POLLER_H

#include <string>

namespace angel {

class evloop;

// 封装底层的I/O multiplexing，为上层提供一个统一的接口
class poller {
public:
    virtual ~poller() {  };
    virtual int wait(evloop *loop, int64_t timeout) = 0;
    virtual void add(int fd, int events) = 0;
    virtual void change(int fd, int events) = 0;
    // 为remove()添加events参数是出于kqueue的需要
    virtual void remove(int fd, int events) = 0;

    void set_name(const char *name) { poller_name = name; }
    const char *name() { return poller_name.c_str(); }
private:
    std::string poller_name;
};
}

#endif // _ANGEL_POLLER_H
