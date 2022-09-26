#ifndef _ANGEL_SIGNALER_H
#define _ANGEL_SIGNALER_H

#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include <angel/channel.h>

namespace angel {

class evloop;

typedef std::function<void()> signaler_handler_t;

//
// We use socketpair() to convert fully asynchronous signals
// into synchronous I/O events for processing.
//
// We register fd[0](the read end of the pipe) in evloop，
// and then whenever a signal is captured, we write one byte to fd[1](the write end of the pipe),
// which is actually the signal value of the signal.
//
// Then when we listen to the occurrence of a readable event with sig_channel in loop,
// we call the different signal processing functions registered by the user
// according to the read signal value.
//
class signaler_t {
public:
    explicit signaler_t(evloop *);
    ~signaler_t();
    signaler_t(const signaler_t&) = delete;
    signaler_t& operator=(const signaler_t&) = delete;

    // if handler == nullptr, ignore signo
    void add_signal(int signo, const signaler_handler_t handler);
    // Restore the default semantics of signo
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
