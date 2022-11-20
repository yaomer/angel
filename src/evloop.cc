#include <angel/evloop.h>

#include <angel/socket.h>
#include <angel/sockops.h>
#include <angel/logger.h>
#include <angel/config.h>

#include "dispatcher.h"
#include "timer.h"

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

namespace {
    // Ensure that each thread can only run one event loop at a time.
    thread_local evloop *this_thread_loop = nullptr;
}

evloop::evloop()
    : timer(new timer_t(this)),
    is_quit(false),
    cur_tid(std::this_thread::get_id())
{
#if defined (ANGEL_HAVE_EPOLL)
    dispatcher.reset(new epoll_base_t);
#elif defined (ANGEL_HAVE_KQUEUE)
    dispatcher.reset(new kqueue_base_t);
#elif defined (ANGEL_HAVE_POLL)
    dispatcher.reset(new poll_base_t);
#elif defined (ANGEL_HAVE_SELECT)
    dispatcher.reset(new select_base_t);
#else
    log_fatal("No supported I/O multiplexing");
#endif
    if (this_thread_loop) {
        log_fatal("Only have one evloop in this thread");
    } else {
        this_thread_loop = this;
    }
    log_info("Use I/O multiplexing (%s)", dispatcher->name());
    sockops::socketpair(wake_pair);
}

evloop::~evloop()
{
    this_thread_loop = nullptr;
}

void evloop::add_channel(channel_ptr chl)
{
    run_in_loop([this, chl = std::move(chl)]{ this->add_channel_in_loop(std::move(chl)); });
}

void evloop::remove_channel(channel_ptr chl)
{
    run_in_loop([this, chl = std::move(chl)]{ this->remove_channel_in_loop(std::move(chl)); });
}

void evloop::remove_channel(int fd)
{
    run_in_loop([this, fd]{ this->remove_channel_in_loop(fd); });
}

void evloop::add_channel_in_loop(channel_ptr chl)
{
    auto r = channel_map.emplace(chl->fd(), chl);
    if (r.second) {
        chl->enable_read();
        log_debug("Add channel(fd=%d) to loop", chl->fd());
    } else {
        log_warn("Try to add duplicate channel(fd=%d) to loop", chl->fd());
    }
}

void evloop::remove_channel_in_loop(channel_ptr chl)
{
    log_debug("Remove channel(fd=%d) from loop", chl->fd());
    chl->disable_all();
    channel_map.erase(chl->fd());
}

void evloop::remove_channel_in_loop(int fd)
{
    auto it = channel_map.find(fd);
    if (it == channel_map.end()) return;
    log_debug("Remove channel(fd=%d) from loop", fd);
    it->second->disable_all();
    channel_map.erase(fd);
}

void evloop::run()
{
    wakeup_init();
    while (!is_quit) {
        int64_t timeout = timer->timeout();
        int nevents = dispatcher->wait(this, timeout);
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

    // Ensure that all tasks are executed when exiting.
    while (true) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (functors.empty()) break;
        }
        do_functors();
    }
    wakeup_close();
}

void evloop::do_functors()
{
    std::vector<functor> tfuncs;
    {
        std::lock_guard<std::mutex> lk(mtx);
        if (!functors.empty()) {
            // It can't be executed here, otherwise deadlock may occur.
            // (such as queue_in_loop())
            tfuncs.swap(functors);
        }
    }
    if (!tfuncs.empty()) {
        for (auto& f : tfuncs) f();
    }
}

void evloop::wakeup_init()
{
    auto chl = std::make_shared<channel>(this);
    chl->set_fd(wake_pair[0]);
    chl->set_read_handler([this]{ this->wakeup_read(); });
    add_channel(std::move(chl));
}

void evloop::wakeup_close()
{
    assert(is_io_loop_thread());
    remove_channel_in_loop(wake_pair[0]);
    close(wake_pair[0]);
    close(wake_pair[1]);
}

// Wakeup current io loop thread
void evloop::wakeup(uint8_t v)
{
    // Multi-thread concurrently write is safe here.
    // We have no writing order requirement, just write one byte into the pair.
    ssize_t n = write(wake_pair[1], &v, sizeof(v));
    if (n != sizeof(v)) {
        log_error("write(): %s", strerrno());
    }
}

void evloop::wakeup_read()
{
    uint8_t v;
    ssize_t n = read(wake_pair[0], &v, sizeof(v));
    if (n > 0) {
        if (v) is_quit = true;
    } else {
        log_error("read(): %s", strerrno());
    }
}

bool evloop::is_io_loop_thread()
{
    return std::this_thread::get_id() == cur_tid;
}

void evloop::run_in_loop(const functor cb)
{
    if (!is_io_loop_thread()) {
        queue_in_loop(std::move(cb));
    } else {
        cb();
    }
}

void evloop::queue_in_loop(const functor cb)
{
    std::lock_guard<std::mutex> lk(mtx);
    functors.emplace_back(std::move(cb));
    // We don't have to wakeup() every time,
    // just wakeup() when the first task is queued.
    //
    // WARNING: If you wakeup() every time, but evloop don't wakeup_read() in time,
    // the wake_pair buffer may be filled, and when you wakeup() again,
    // the wakeup thread will be blocked.
    if (functors.size() == 1) {
        wakeup(0);
    }
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
    wakeup(1);
}

}
