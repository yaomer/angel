#ifndef _ANGEL_SIGNALER_H
#define _ANGEL_SIGNALER_H

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include "Channel.h"
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class EventLoop;

// Signaler用来处理信号事件，但我们实际上是用socketpair将其转化为
// I/O事件来处理的，这就将信号的异步转化为了同步，从而简化了处理
//
// 我们将管道的一端(fd[0])注册到eventloop中，然后每当有信号被捕获时，
// 我们就向fd[1]中写入1 byte，这1 byte实际上就是该信号的信号值，
// 然后当我们在loop中监听到有sigChannel的可读事件发生时，我们就会
// 根据读到的信号值去调用用户注册的不同的信号处理函数
class Signaler : noncopyable {
public:
    explicit Signaler(EventLoop *);
    ~Signaler() {  };
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
