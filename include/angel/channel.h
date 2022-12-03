#ifndef __ANGEL_CHANNEL_H
#define __ANGEL_CHANNEL_H

#include <functional>

namespace angel {

class evloop;

enum event_type {
    Read    = 0x01,
    Write   = 0x02,
    Error   = 0x04,
};

typedef std::function<void()> event_handler_t;

// channel is used to abstract an I/O event
// and handle different I/O events according to the registered handler.
//
// It is the base event in the event loop of the reactor mode.
//
// The ownership of channel is held by evloop.
//
class channel {
public:
    // channel holds fd by default,
    // that is to say, it will close(fd) in destructor.
    // If you don't want this, set `hold_fd` to false.
    channel(evloop *loop, int fd, bool hold_fd = true);
    ~channel();

    channel(const channel&) = delete;
    channel& operator=(const channel&) = delete;

    int fd() const { return evfd; }
    evloop *get_loop() const { return loop; }

    // add() and remove() should be used together,
    // and MUST NOT delete channel* after add().

    // Add channel to the loop.
    // Read event is enabled by default.
    // To avoid busy-loop, you should enable Write event when needed.
    void add();
    // Disable all associated events on fd, and removed from the loop.
    void remove();

    // After channel is added to the loop,
    // the following member functions must be called in loop thread.
    bool is_reading() const { return filter & Read; }
    bool is_writing() const { return filter & Write; }
    void enable_read();
    void enable_write();
    void disable_read();
    void disable_write();

    void set_read_handler(event_handler_t handler);
    void set_write_handler(event_handler_t handler);
    void set_error_handler(event_handler_t handler);
private:
    void disable_all();
    void handle_event();

    evloop *loop;
    const int evfd;
    bool hold_fd;
    int filter;  // Events of interest associated with evfd.
    int trigger; // Events that have been triggered, returned by I/O Multiplexing.
    event_handler_t read_handler;
    event_handler_t write_handler;
    event_handler_t error_handler;

    // Used to modify trigger.
    friend class select_base_t;
    friend class poll_base_t;
    friend class kqueue_base_t;
    friend class epoll_base_t;
    friend class evloop;
};

}

#endif // __ANGEL_CHANNEL_H
