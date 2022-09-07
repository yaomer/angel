#ifndef _ANGEL_SIGNALER_H
#define _ANGEL_SIGNALER_H

#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include "channel.h"

namespace angel {

class evloop;

typedef std::function<void()> signaler_handler_t;

// signaler用来处理信号事件，但我们实际上是用socketpair将其转化为
// I/O事件来处理的，这就将信号的异步转化为了同步，从而简化了处理
//
// 我们将管道的一端(fd[0])注册到evloop中，然后每当有信号被捕获时，
// 我们就向fd[1]中写入1 byte，这1 byte实际上就是该信号的信号值，
// 然后当我们在loop中监听到有sig_channel的可读事件发生时，我们就会
// 根据读到的信号值去调用用户注册的不同的信号处理函数
class signaler_t {
public:
    explicit signaler_t(evloop *);
    ~signaler_t();
    signaler_t(const signaler_t&) = delete;
    signaler_t& operator=(const signaler_t&) = delete;

    void add_signal(int signo, const signaler_handler_t handler);
    void cancel_signal(int signo);
    void start();
private:
    void add_signal_in_loop(int signo, const signaler_handler_t handler);
    void cancel_signal_in_loop(int signo);
    void sig_catch();
    static void sig_handler(int signo);

    evloop *loop;
    std::shared_ptr<channel> sig_channel;
    std::map<int, signaler_handler_t> sig_callback_map;
    int pair_fd[2];
};

extern std::mutex _SYNC_SIG_INIT_LOCK;

extern angel::signaler_t *__signaler_ptr;

void add_signal(int signo, const signaler_handler_t handler);
void cancel_signal(int signo);

}

#endif // _ANGEL_SIGNALER_H
