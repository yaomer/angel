#ifndef _ANGEL_SIGNALER_H
#define _ANGEL_SIGNALER_H

#include <functional>
#include <map>
#include <memory>
#include "Channel.h"
#include "Noncopyable.h"

namespace Angel {

class EventLoop;

class Signaler : Noncopyable {
public:
    typedef std::function<void()> SignalerCallback;
    explicit Signaler(EventLoop *);
    ~Signaler();
    void add(int signo, const SignalerCallback _cb);
    void cancel(int signo);
    void sigCatch(std::shared_ptr<Channel>, Buffer&);
    void start();
    static void sigHandler(int signo);
private:
    EventLoop *_loop;
    Channel *_sigChannel;
    std::map<int, SignalerCallback> _sigCallbackMaps;
    int _pairFd[2];
};

}

#endif // _ANGEL_SIGNALER_H
