#ifndef __ANGEL_EVLOOP_H
#define __ANGEL_EVLOOP_H

#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>

#include <angel/channel.h>

namespace angel {

class dispatcher;

class timer_t;
typedef std::function<void()> timer_callback_t;

typedef std::function<void()> functor;

////////////////////////////////////////
// The event loop, the core of angel. //
////////////////////////////////////////
class evloop {
public:
    evloop();
    ~evloop();

    evloop(const evloop&) = delete;
    evloop& operator=(const evloop&) = delete;

    // Run the event loop.
    // This will enter an infinite loop to poll events until quit() is called.
    void run();
    // Quit the event loop. (thread-safe)
    // It will return immediately without waiting for the evloop to exit.
    void quit();

    ////////////////////////////////////////////////////////////
    // The following public member functions are thread-safe. //
    ////////////////////////////////////////////////////////////

    // Current thread is io loop thread ?
    bool is_io_loop_thread();
    // Execute user callback on io loop thread.
    // Execute immediately if is_io_loop_thread() else queue_in_loop().
    void run_in_loop(functor cb);
    // Put the cb into the task queue of the io loop thread.
    void queue_in_loop(functor cb);

    // Execute the cb after timeout (ms),
    // and a timer id is returned, that can be used to cancel a timer.
    size_t run_after(int64_t timeout_ms, timer_callback_t cb);
    // Execute the cb every interval (ms).
    // and a timer id is returned, that can be used to cancel a timer.
    size_t run_every(int64_t interval_ms, timer_callback_t cb);
    // Cancel a timer by id.
    void cancel_timer(size_t id);
private:
    void do_functors();

    void wakeup_init();
    void wakeup_close();
    void wakeup_read();
    void wakeup(uint8_t);

    // Used by dispatcher.
    channel *search_channel(int fd);

    std::unique_ptr<dispatcher> dispatcher;
    std::unique_ptr<timer_t> timer;
    std::unordered_map<int, std::unique_ptr<channel>> channel_map;
    std::vector<channel*> active_channels;
    const std::thread::id cur_tid;
    // A task queue for transferring tasks
    // from non-io threads to io threads for execution.
    std::vector<functor> functors;
    std::mutex mtx;
    int wake_pair[2];
    channel *wake_channel;
    bool is_quit;

    friend class select_base_t;
    friend class poll_base_t;
    friend class kqueue_base_t;
    friend class epoll_base_t;
    friend class channel;
};

} // angel

#endif // __ANGEL_EVLOOP_H
