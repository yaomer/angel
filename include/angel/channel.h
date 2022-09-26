#ifndef _ANGEL_CHANNEL_H
#define _ANGEL_CHANNEL_H

#include <functional>

namespace angel {

class evloop;

enum event_type {
    Read    = 0x01,
    Write   = 0x02,
    Error   = 0x04,
};

//
// Channel is used to abstract an I/O event
// and handle different I/O events according to the registered callback.
//
// It is the event in the event loop of the reactor mode.
//
// When using channels, read events are generally registered by default,
// and only write events are registered when needed.
//
// Channel does not hold fd, that is to say, it will not close(fd) in destructor.
//
class channel {
public:
    typedef std::function<void()> event_handler_t;

    explicit channel(evloop *);
    ~channel();
    channel(const channel&) = delete;
    channel& operator=(const channel&) = delete;

    int fd() const { return evfd; }
    void set_fd(int fd) { evfd = fd; }
    int filter_events() const { return filter; }
    int trigger_events() const { return trigger; }
    void set_trigger_events(int revents) { trigger = revents; }
    bool is_reading() { return filter & Read; }
    bool is_writing() { return filter & Write; }
    void enable_read();
    void enable_write();
    void disable_write();
    void set_read_handler(const event_handler_t handler)
    { read_handler = std::move(handler); }
    void set_write_handler(const event_handler_t handler)
    { write_handler = std::move(handler); }
    void set_error_handler(const event_handler_t handler)
    { error_handler = std::move(handler); }
    void handle_event();
private:

    evloop *loop;
    int evfd;
    int filter; // the event registered to evfd
    int trigger; // triggered events returned by the kernel
    event_handler_t read_handler;
    event_handler_t write_handler;
    event_handler_t error_handler;
};

}

#endif // _ANGEL_CHANNEL_H
