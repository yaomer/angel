#include <angel/evloop.h>

#include <angel/socket.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/config.h>

#include "poller.h"
#include "timer.h"
#include "signaler.h"

#if defined (ANGEL_HAVE_KQUEUE)
#include "kqueue.h"
#endif
#if defined (ANGEL_HAVE_EPOLL)
#include "epoll.h"
#endif
#if defined (ANGEL_HAVE_POLL)
#include "poll.h"
#endif
#if defined (ANGEL_HAVE_SELECT)
#include "select.h"
#endif

namespace angel {

using namespace util;

static thread_local evloop *this_thread_loop = nullptr;

evloop::evloop()
    : timer(new timer_t(this)),
    is_quit(false),
    cur_tid(std::this_thread::get_id())
{
#if defined (ANGEL_HAVE_EPOLL)
    poller.reset(new epoll_base_t);
#elif defined (ANGEL_HAVE_KQUEUE)
    poller.reset(new kqueue_base_t);
#elif defined (ANGEL_HAVE_POLL)
    poller.reset(new poll_base_t);
#elif defined (ANGEL_HAVE_SELECT)
    poller.reset(new select_base_t);
#else
    log_fatal("No supported I/O multiplexing");
#endif
    if (this_thread_loop) {
        log_fatal("Only have one evloop in this thread");
    } else {
        this_thread_loop = this;
    }
    log_info("Use I/O multiplexing (%s)", poller->name());
    sockops::socketpair(wake_fd);
    std::lock_guard<std::mutex> mlock(_SYNC_SIG_INIT_LOCK);
    if (!__signaler_ptr) {
        signaler.reset(new signaler_t(this));
        __signaler_ptr = signaler.get();
    }
}

evloop::~evloop()
{
}

void evloop::add_channel(const channel_ptr& chl)
{
    run_in_loop([this, chl]{ this->add_channel_in_loop(chl); });
}

void evloop::remove_channel(const channel_ptr& chl)
{
    run_in_loop([this, chl]{ this->remove_channel_in_loop(chl); });
}

void evloop::add_channel_in_loop(const channel_ptr& chl)
{
    chl->enable_read();
    auto it = channel_map.emplace(chl->fd(), chl);
    if (it.second) {
        log_debug("Add channel(fd=%d) to loop", chl->fd());
    } else {
        log_warn("channel(fd=%d) has already exists", chl->fd());
    }
}

void evloop::remove_channel_in_loop(const channel_ptr& chl)
{
    log_debug("Remove channel(fd=%d) from loop", chl->fd());
    chl->disable_all();
    channel_map.erase(chl->fd());
}

void evloop::run()
{
    wakeup_init();
    if (signaler) signaler->start();
    while (!is_quit) {
        int64_t timeout = timer->timeout();
        int nevents = poller->wait(this, timeout);
        if (nevents > 0) {
            for (auto& channel : active_channels) {
                channel->handle_event();
            }
            active_channels.clear();
        } else if (nevents == 0) {
            timer->tick();
        }
        do_functors();
    }
}

void evloop::do_functors()
{
    std::vector<functor> tfuncs;
    {
        std::lock_guard<std::mutex> mlock(mutex);
        if (!functors.empty()) {
            tfuncs.swap(functors);
            functors.clear();
        }
    }
    for (auto& func : tfuncs)
        func();
}

void evloop::wakeup_init()
{
    auto chl = channel_ptr(new channel(this));
    chl->set_fd(wake_fd[0]);
    chl->set_read_handler([this]{ this->handle_read(); });
    add_channel(chl);
}

// Wakeup current io loop thread
void evloop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wake_fd[1], &one, sizeof(one));
    if (n != sizeof(one)) {
        log_error("Write (%zd) bytes instead of (%zu)", n, sizeof(one));
    }
}

void evloop::handle_read()
{
    uint64_t one;
    ssize_t n = read(wake_fd[0], &one, sizeof(one));
    if (n != sizeof(one)) {
        log_error("Read (%zd) bytes instead of (%zu)", n, sizeof(one));
    }
}

bool evloop::is_io_loop_thread()
{
    return std::this_thread::get_id() == cur_tid;
}

void evloop::run_in_loop(const functor cb)
{
    if (!is_io_loop_thread()) {
        queue_in_loop(cb);
    } else {
        cb();
    }
}

void evloop::queue_in_loop(const functor cb)
{
    {
        std::lock_guard<std::mutex> mlock(mutex);
        functors.emplace_back(cb);
    }
    wakeup();
}

size_t evloop::run_after(int64_t timeout, const timer_callback_t cb)
{
    int64_t expire = get_cur_time_ms() + timeout;
    timer_task_t *task = new timer_task_t(expire, 0, std::move(cb));
    size_t id = timer->add_timer(task);
    log_debug("Add a timer(id=%zu) after %lld ms", id, timeout);
    return id;
}

size_t evloop::run_every(int64_t interval, const timer_callback_t cb)
{
    int64_t expire = get_cur_time_ms() + interval;
    timer_task_t *task = new timer_task_t(expire, interval, std::move(cb));
    size_t id = timer->add_timer(task);
    log_debug("Add a timer(id=%zu) every %lld ms", id, interval);
    return id;
}

void evloop::cancel_timer(size_t id)
{
    timer->cancel_timer(id);
    log_debug("Cancel a timer(id=%zu)", id);
}

void evloop::quit()
{
    is_quit = true;
    wakeup();
}

}
