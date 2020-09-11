#include "evloop.h"
#include "socket.h"
#include "sockops.h"
#include "config.h"
#include "logger.h"

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

using namespace angel;
using namespace angel::util;

namespace angel {

    thread_local evloop *this_thread_loop = nullptr;
}

evloop::evloop()
    : timer(new timer_t(this)),
    is_quit(false),
    cur_tid(std::this_thread::get_id()),
    nloops(0)
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
    } else
        this_thread_loop = this;
    sockops::socketpair(wake_fd);
    std::lock_guard<std::mutex> mlock(_SYNC_SIG_INIT_LOCK);
    if (!__signaler_ptr) {
        signaler.reset(new signaler_t(this));
        __signaler_ptr = signaler.get();
    }
}

void evloop::add_channel(const channel_ptr& chl)
{
    log_debug("add channel(fd=%d)...", chl->fd());
    run_in_loop([this, chl]{ this->add_channel_in_loop(chl); });
}

void evloop::remove_channel(const channel_ptr& chl)
{
    log_debug("remove channel(fd=%d)...", chl->fd());
    run_in_loop([this, chl]{ this->remove_channel_in_loop(chl); });
}

void evloop::add_channel_in_loop(const channel_ptr& chl)
{
    log_debug("channel(fd=%d) has been added to the loop", chl->fd());
    chl->enable_read();
    poller->add(chl->fd(), chl->filter_events());
    channel_map.emplace(chl->fd(), chl);
}

void evloop::remove_channel_in_loop(const channel_ptr& chl)
{
    log_debug("channel(fd=%d) has been removed from the loop", chl->fd());
    poller->remove(chl->fd(), chl->filter_events());
    channel_map.erase(chl->fd());
}

void evloop::run()
{
    wakeup_init();
    if (signaler) signaler->start();
    while (!is_quit) {
        int64_t timeout = timer->timeout();
        int nevents = poller->wait(this, timeout);
        log_debug("nloops=%zu, timeout=%lld, nevents=%d", nloops++, timeout, nevents);
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

// 唤醒io线程
void evloop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wake_fd[1], &one, sizeof(one));
    if (n != sizeof(one)) {
        log_error("write %zd bytes instead of %zu", n, sizeof(one));
    } else
        log_debug("wake up the io-loop by fd=%d", wake_fd[1]);
}

// 接受被唤醒
void evloop::handle_read()
{
    uint64_t one;
    ssize_t n = read(wake_fd[0], &one, sizeof(one));
    if (n != sizeof(one))
        log_error("read %zd bytes instead of %zu", n, sizeof(one));
}

// 判断当前线程是否是io线程
bool evloop::is_io_loop_thread()
{
    return std::this_thread::get_id() == cur_tid;
}

// 在io线程执行用户回调
void evloop::run_in_loop(const functor cb)
{
    if (!is_io_loop_thread()) {
        queue_in_loop(cb);
    } else
        cb();
}

// 向任务队列中添加一个任务
void evloop::queue_in_loop(const functor cb)
{
    {
        std::lock_guard<std::mutex> mlock(mutex);
        functors.emplace_back(cb);
    }
    wakeup();
}

// [timeout ms]后执行一次cb
size_t evloop::run_after(int64_t timeout, const timer_callback_t cb)
{
    int64_t expire = get_cur_time_ms() + timeout;
    timer_task_t *task = new timer_task_t(expire, 0, std::move(cb));
    size_t id = timer->add_timer(task);
    log_info("add a timer(id=%zu) after %lld ms", id, timeout);
    return id;
}

// 每隔[interval ms]执行一次cb
size_t evloop::run_every(int64_t interval, const timer_callback_t cb)
{
    int64_t expire = get_cur_time_ms() + interval;
    timer_task_t *task = new timer_task_t(expire, interval, std::move(cb));
    size_t id = timer->add_timer(task);
    log_info("add a timer(id=%zu) every %lld ms", id, interval);
    return id;
}

void evloop::cancel_timer(size_t id)
{
    timer->cancel_timer(id);
    log_info("cancel a timer(id=%zu)", id);
}

void evloop::quit()
{
    is_quit = true;
    wakeup();
}
