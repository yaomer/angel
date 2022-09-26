#include "signaler.h"

#include <signal.h>
#include <unistd.h>
#include <string.h>

#include <unordered_map>

#include <angel/evloop.h>
#include <angel/socket.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

// Protect __signaler_ptr to initialize properly.
//
// Due to the particularity of signals, there can only be one signaler instance per process,
// that is, it can only be bound to one evloop.
//
std::mutex _SYNC_SIG_INIT_LOCK;
angel::signaler_t *__signaler_ptr = nullptr;

static int signal_fd = -1;

std::unordered_map<int, const char*> signal_map = {
    {  1, "SIGHUP"  },
    {  2, "SIGINT"  },
    {  3, "SIGQUIT" },
    { 13, "SIGPIPE" },
    { 15, "SIGTERM" },
};

static const char *signal_str(int signo)
{
    auto it = signal_map.find(signo);
    if (it != signal_map.cend())
        return it->second;
    else
        return "unknown-signo";
}

signaler_t::signaler_t(evloop *loop)
    : loop(loop),
    sig_channel(new channel(loop))
{
    if (signal_fd != -1)
        log_fatal("Only have one signaler instance in one process");
    sockops::socketpair(pair_fd);
    sig_channel->set_fd(pair_fd[0]);
    signal_fd = pair_fd[1];
    sig_channel->set_read_handler([this]{ this->sig_catch(); });
}

signaler_t::~signaler_t() = default;

void signaler_t::start()
{
    log_debug("signaler is running");
    loop->add_channel(sig_channel);
}

void signaler_t::sig_handler(int signo)
{
    ssize_t n = write(signal_fd,
                      reinterpret_cast<void *>(&signo), 1);
    log_info("%s is triggered", signal_str(signo));
    if (n != 1)
        log_error("write %zd bytes instead of 1");
}

void signaler_t::add_signal(int signo, const signaler_handler_t handler)
{
    loop->run_in_loop(
            [this, signo, handler]{ this->add_signal_in_loop(signo, handler); });
}

void signaler_t::cancel_signal(int signo)
{
    loop->run_in_loop(
            [this, signo]{ this->cancel_signal_in_loop(signo); });
}

void signaler_t::add_signal_in_loop(int signo, const signaler_handler_t handler)
{
    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    if (handler) {
        sa.sa_handler = &sig_handler;
        sig_callback_map.emplace(signo, std::move(handler));
        log_debug("set sig_handler for %s", signal_str(signo));
    } else {
        sa.sa_handler = SIG_IGN;
        log_debug("ignore %s", signal_str(signo));
    }
    // Restart syscall interrupted by signal
    sa.sa_flags |= SA_RESTART;
    // Mask all signals before building sig handler
    sigfillset(&sa.sa_mask);
    if (sigaction(signo, &sa, nullptr) < 0)
        log_error("sigaction: %s", strerrno());
}

void signaler_t::cancel_signal_in_loop(int signo)
{
    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sig_callback_map.erase(signo);
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(signo, &sa, nullptr) < 0) {
        log_error("sigaction: %s", strerrno());
    } else {
        log_debug("restores the default semantics of %s", signal_str(signo));
    }
}

// Call the corresponding processing function according to the read signal value.
void signaler_t::sig_catch()
{
    static unsigned char buf[1024];
    bzero(buf, sizeof(buf));
    ssize_t n = read(sig_channel->fd(), buf, sizeof(buf));
    if (n < 0) log_error("read: %s", strerrno());
    for (int i = 0; i < n; i++) {
        auto it = sig_callback_map.find(buf[i]);
        if (it != sig_callback_map.end())
            it->second();
    }
}

void add_signal(int signo, const signaler_handler_t handler)
{
    if (__signaler_ptr)
        __signaler_ptr->add_signal(signo, std::move(handler));
}

void cancel_signal(int signo)
{
    if (__signaler_ptr)
        __signaler_ptr->cancel_signal(signo);
}

}
