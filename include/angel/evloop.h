#ifndef _ANGEL_EVLOOP_H
#define _ANGEL_EVLOOP_H

#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>

#include <angel/channel.h>

namespace angel {

class poller;

class timer_t;
typedef std::function<void()> timer_callback_t;

class signaler_t;
typedef std::function<void()> signaler_handler_t;

typedef std::function<void()> functor;
typedef std::shared_ptr<channel> channel_ptr;

// The event loop, the core of the recator mode.
class evloop {
public:
    evloop();
    ~evloop();
    evloop(const evloop&) = delete;
    evloop& operator=(const evloop&) = delete;

    void add_channel(const channel_ptr& chl);
    void remove_channel(const channel_ptr& chl);

    channel_ptr search_channel(int fd)
    { return channel_map.find(fd)->second; }

    void run();
    bool is_io_loop_thread();
    // Execute user callback on io loop thread.
    // Execute immediately if the current thread is io thread, else queue_in_loop(cb).
    void run_in_loop(const functor cb);
    // Put the callback into the task queue of the io thread.
    void queue_in_loop(const functor cb);
    // Execute a callback after timeout (ms)
    size_t run_after(int64_t timeout_ms, const timer_callback_t cb);
    // Execute a callback every interval (ms)
    size_t run_every(int64_t interval_ms, const timer_callback_t cb);
    void cancel_timer(size_t id);
    void quit();
private:
    void wakeup_init();
    void wakeup();
    void handle_read();
    void add_channel_in_loop(const channel_ptr& chl);
    void remove_channel_in_loop(const channel_ptr& chl);
    void do_functors();

    std::unique_ptr<poller> poller;
    std::unique_ptr<timer_t> timer;
    std::unique_ptr<signaler_t> signaler;
    std::unordered_map<int, std::shared_ptr<channel>> channel_map;
    std::vector<std::shared_ptr<channel>> active_channels;
    std::atomic_bool is_quit;
    std::thread::id cur_tid;
    // A task queue for transferring tasks
    // from non-io threads to io threads for execution.
    std::vector<functor> functors;
    std::mutex mutex;
    int wake_fd[2];

    friend class select_base_t;
    friend class poll_base_t;
    friend class kqueue_base_t;
    friend class epoll_base_t;
    friend class channel;
};

} // angel

#endif // _ANGEL_EVLOOP_H
