#ifndef __ANGEL_SIGNALER_H
#define __ANGEL_SIGNALER_H

#include <functional>
#include <unordered_map>
#include <forward_list>
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

    size_t add_signal(int signo, const signaler_handler_t handler);
    void ignore_siganl(int signo);
    // Restore the default semantics of signo
    void cancel_signal(size_t id);
    void start();
private:
    struct signaler_event {
        size_t sig_id;
        signaler_handler_t sig_cb;
    };

    void add_signal_in_loop(int signo, signaler_event event);
    void ignore_siganl_in_loop(int signo);
    void cancel_signal_in_loop(size_t id);
    void sig_catch();

    evloop *loop;
    std::shared_ptr<channel> sig_channel;
    std::unordered_map<int, std::forward_list<signaler_event>> sig_map;
    std::unordered_map<size_t, int> id_map;
    std::atomic_size_t sig_id;
    int sig_pair[2];
};

}

#endif // __ANGEL_SIGNALER_H
