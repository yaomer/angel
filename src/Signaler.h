#ifndef _ANGEL_SIGNALER_H
#define _ANGEL_SIGNALER_H

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include "Channel.h"
#include "Noncopyable.h"
#include "decls.h"

namespace Angel {

class EventLoop;

class Signaler : Noncopyable {
public:
    explicit Signaler(EventLoop *);
    ~Signaler();
    void add(int signo, const SignalerCallback _cb);
    void cancel(int signo);
    void sigCatch();
    void start();
    static void sigHandler(int signo);
private:
    void addInLoop(int signo, const SignalerCallback _cb);
    void cancelInLoop(int signo);

    EventLoop *_loop;
    std::shared_ptr<Channel> _sigChannel;
    std::map<int, SignalerCallback> _sigCallbackMaps;
    int _pairFd[2];
};

extern std::mutex _SYNC_INIT_LOCK;

extern Angel::Signaler *__signalerPtr;


void addSignal(int signo, const SignalerCallback _cb);
void cancelSignal(int signo);

}

#endif // _ANGEL_SIGNALER_H
