#ifndef _ANGEL_EVLOOP_H
#define _ANGEL_EVLOOP_H

#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <vector>
#include <map>

#include "channel.h"

namespace angel {

class poller;
class select_base_t;
class poll_base_t;
class kqueue_base_t;
class epoll_base_t;

class timer_t;
typedef std::function<void()> timer_callback_t;

class signaler_t;
typedef std::function<void()> signaler_handler_t;

// reactor模式的核心，事件循环的驱动
class evloop {
public:
    typedef std::function<void()> functor;
    typedef std::shared_ptr<channel> channel_ptr;

    evloop();
    ~evloop();
    evloop(const evloop&) = delete;
    evloop& operator=(const evloop&) = delete;
    // 向loop中注册一个事件
    void add_channel(const channel_ptr& chl);
    // 从loop中删除一个事件
    void remove_channel(const channel_ptr& chl);
    // 修改fd上关联的事件
    void change_event(int fd, int events);

    channel_ptr search_channel(int fd)
    { return channel_map.find(fd)->second; }

    void run();
    bool is_io_loop_thread();
    void run_in_loop(const functor cb);
    void queue_in_loop(const functor cb);
    size_t run_after(int64_t timeout_ms, const timer_callback_t cb);
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
    std::map<int, std::shared_ptr<channel>> channel_map;
    std::vector<std::shared_ptr<channel>> active_channels;
    std::atomic_bool is_quit;
    std::thread::id cur_tid;
    // 一个任务队列，用于将非io线程的任务转到io线程执行
    std::vector<functor> functors;
    std::mutex mutex;
    int wake_fd[2];
    size_t nloops;

    friend class select_base_t;
    friend class poll_base_t;
    friend class kqueue_base_t;
    friend class epoll_base_t;
};

} // angel

#endif // _ANGEL_EVLOOP_H
