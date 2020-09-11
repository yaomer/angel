#ifndef _ANGEL_CHANNEL_H
#define _ANGEL_CHANNEL_H

#include <functional>

#include "event.h"
#include "noncopyable.h"
#include "enum_bitwise.h"

namespace angel {

class evloop;

// channel用来抽象一个I/O事件，根据注册回调的不同来处理不同的I/O事件
// 它是基于reactor模式的事件循环中的事件
//
// 使用channel时一般会默认注册读事件，而只在需要时注册写事件
//
// channel并不持有fd，也就是说析构时不会去close(fd)
//
class channel : noncopyable {
public:
    typedef std::function<void()> event_handler_t;

    explicit channel(evloop *);
    ~channel() {  };
    int fd() const { return evfd; }
    void set_fd(int fd) { evfd = fd; }
    event filter_events() const { return filter; }
    event trigger_events() const { return trigger; }
    void set_trigger_events(event revents) { trigger = revents; }
    bool is_reading() { return is_enum_true(filter & event::read); }
    bool is_writing() { return is_enum_true(filter & event::write); }
    void enable_read() { filter |= event::read; }
    void enable_write() { filter |= event::write; change_events(); }
    void disable_write() { filter &= ~event::write; change_events(); }
    void set_read_handler(const event_handler_t handler)
    { read_handler = std::move(handler); }
    void set_write_handler(const event_handler_t handler)
    { write_handler = std::move(handler); }
    void set_error_handler(const event_handler_t handler)
    { error_handler = std::move(handler); }
    void handle_event();
private:
    void change_events();
    const char *ev2str(event events);

    evloop *loop;
    int evfd;
    event filter; // 用户关注的事件
    event trigger; // 由内核返回的已经发生的事件
    event_handler_t read_handler;
    event_handler_t write_handler;
    event_handler_t error_handler;
};

}

#endif // _ANGEL_CHANNEL_H
