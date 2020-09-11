#ifndef _ANGEL_EVLOOP_H
#define _ANGEL_EVLOOP_H

#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <vector>
#include <map>

#include "poller.h"
#include "channel.h"
#include "timer.h"
#include "signaler.h"
#include "noncopyable.h"

namespace angel {

// reactor模式的核心，事件循环的驱动
class evloop : noncopyable {
public:
    typedef std::function<void()> functor;
    typedef std::shared_ptr<channel> channel_ptr;

    evloop();
    // 向loop中注册一个事件
    void add_channel(const channel_ptr& chl);
    // 从loop中删除一个事件
    void remove_channel(const channel_ptr& chl);

    void change_event(int fd, event events)
    { poller->change(fd, events); }

    channel_ptr search_channel(int fd)
    { return channel_map.find(fd)->second; }

    void fill_active_channel(channel_ptr& chl)
    { active_channels.emplace_back(chl); }

    void run();
    void wakeup();
    bool is_io_loop_thread();
    void run_in_loop(const functor cb);
    void queue_in_loop(const functor cb);
    size_t run_after(int64_t timeout_ms, const timer_callback_t cb);
    size_t run_every(int64_t interval_ms, const timer_callback_t cb);
    void cancel_timer(size_t id);
    void quit();
private:
    void wakeup_init();
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
};

} // angel

#endif // _ANGEL_EVLOOP_H
