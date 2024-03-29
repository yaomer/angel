#include <angel/signal.h>

#include <unistd.h>
#include <string.h>

#include <unordered_map>
#include <forward_list>

#include <angel/evloop_thread.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

static int sig_fd = -1;

//
// We use socketpair() to convert async-signals into sync I/O events for processing.
//
// We register fd[0](the read end of the pipe) in evloop, and then whenever a signal
// is captured, we write one byte to fd[1](the write end of the pipe),
// which is actually the signal value of the signal.
//
// Then when we listen to the occurrence of a readable event with signal channel
// in loop, we call the different signal handler registered by the user
// according to the read signal value.
//
class signaler_t {
public:
    signaler_t(const signaler_t&) = delete;
    signaler_t& operator=(const signaler_t&) = delete;

    static signaler_t& get_signaler()
    {
        static signaler_t signaler;
        return signaler;
    }

    size_t add_signal(int signo, const signal_handler_t handler);
    void ignore_siganl(int signo);
    // Restore the default semantics of signo
    void cancel_signal(size_t id);
    void start();
private:
    signaler_t();
    ~signaler_t();

    struct signaler_event {
        size_t sig_id;
        signal_handler_t sig_cb;
    };

    void add_signal_in_loop(int signo, signaler_event event);
    void ignore_siganl_in_loop(int signo);
    void cancel_signal_in_loop(size_t id);
    void sig_catch();

    evloop *loop;
    // Due to the particularity of signals, there can only be one signaler instance
    // per process, that is, it can only be bound to one evloop.
    // So we use a special thread to process signals.
    evloop_thread sig_thread;
    std::unordered_map<int, std::forward_list<signaler_event>> sig_map;
    std::unordered_map<size_t, int> id_map;
    std::atomic_size_t sig_id;
    channel *sig_channel;
    int sig_pair[2];
};

signaler_t::signaler_t()
{
    if (sig_fd != -1)
        log_fatal("Only have one signaler instance in one process");
    loop = sig_thread.get_loop();
    sockops::socketpair(sig_pair);
    sig_channel = new channel(loop, sig_pair[0]);
    sig_channel->set_read_handler([this]{ this->sig_catch(); });
    sig_channel->add();
    sig_fd = sig_pair[1];
}

signaler_t::~signaler_t()
{
    log_debug("~signaler_t()");
}

// Only async-signal-safe functions can be called in signal handler.
//
// In particular, nonreentrant functions are generally unsafe to
// call from a signal handler. It's also not safe that involves
// dynamic memory allocation and locking.
//
// All functions of stdio library are not async-signal-safe.
//
// This is easy to understand, when performing buffered I/O on a file,
// the stdio functions must maintain a statically allocated data buffer
// along with associated counters and indexes (or pointers) that record
// the amount of data and the current position in the buffer.
//
// Suppose that the main program is in the middle of a call to a stdio
// function such as printf(3) where the buffer and associated variables
// have been partially updated. If, at that moment, the program is
// interrupted by a signal handler that also calls printf(3), then
// the second call to printf(3) will operate on inconsistent data,
// with unpredictable results.
//
static void sig_handler(int signo)
{
    // Write only the signal value triggerred here to wake up the io loop,
    // and then to execute the corresponding signal handler (in sig_catch)
    // according to the read signal value in the io thread loop.
    write(sig_fd, reinterpret_cast<void *>(&signo), 1);
}

static void sigaction(int signo, void (*handler)(int))
{
    struct sigaction sa;
    bzero(&sa, sizeof(sa));

    sa.sa_handler = handler;
    // Restart syscall interrupted by signal.
    sa.sa_flags |= SA_RESTART;
    // Mask all signals before building sig handler.
    sigfillset(&sa.sa_mask);

    if (::sigaction(signo, &sa, nullptr) < 0) {
        log_error("sigaction: %s", strerrno());
    }
}

size_t signaler_t::add_signal(int signo, const signal_handler_t handler)
{
    size_t id = sig_id.fetch_add(1, std::memory_order_relaxed);

    signaler_event event;
    event.sig_id = id;
    event.sig_cb = std::move(handler);

    loop->run_in_loop([this, signo, event = std::move(event)]{
            this->add_signal_in_loop(signo, std::move(event));
            });
    return id;
}

void signaler_t::ignore_siganl(int signo)
{
    loop->run_in_loop([this, signo]{ this->ignore_siganl_in_loop(signo); });
}

void signaler_t::cancel_signal(size_t id)
{
    loop->run_in_loop([this, id]{ this->cancel_signal_in_loop(id); });
}

void signaler_t::add_signal_in_loop(int signo, signaler_event event)
{
    auto& sl = sig_map[signo];
    if (sl.empty()) {
        // The signal handler of each newly added signal
        // will be set to sig_handler.
        sigaction(signo, sig_handler);
    }
    id_map.emplace(event.sig_id, signo);
    sl.emplace_front(std::move(event));
}

void signaler_t::ignore_siganl_in_loop(int signo)
{
    for (auto& event : sig_map[signo]) {
        id_map.erase(event.sig_id);
    }
    sig_map.erase(signo);
    sigaction(signo, SIG_IGN);
}

void signaler_t::cancel_signal_in_loop(size_t id)
{
    auto it = id_map.find(id);
    if (it == id_map.end()) return;

    int signo = it->second;
    id_map.erase(it);

    auto iter = sig_map.find(signo);
    Assert(iter != sig_map.end());
    auto& sl = iter->second;
    auto curr = sl.cbegin();
    auto prev = sl.cbefore_begin();
    for ( ; curr != sl.cend(); ++curr, ++prev) {
        if (id == curr->sig_id) {
            sl.erase_after(prev);
            break;
        }
    }

    if (sl.empty()) {
        sig_map.erase(iter);
        sigaction(signo, SIG_DFL);
    }
}

void signaler_t::sig_catch()
{
    static unsigned char buf[1024];

    ssize_t n = read(sig_pair[0], buf, sizeof(buf));
    if (n < 0) log_error("read: %s", strerrno());

    for (int i = 0; i < n; i++) {
        int signo = buf[i];
        auto it = sig_map.find(signo);
        Assert(it != sig_map.end());
        log_info("Sig[%d] is triggered: %s", signo, strsignal(signo));
        for (auto& event : it->second) {
            event.sig_cb();
        }
    }
}

size_t add_signal(int signo, const signal_handler_t handler)
{
    return signaler_t::get_signaler().add_signal(signo, std::move(handler));
}

void cancel_signal(size_t id)
{
    signaler_t::get_signaler().cancel_signal(id);
}

void ignore_signal(int signo)
{
    signaler_t::get_signaler().ignore_siganl(signo);
}

}
