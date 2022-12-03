#include <angel/channel.h>

#include <unistd.h>

#include <angel/evloop.h>
#include <angel/util.h>
#include <angel/logger.h>

#include "dispatcher.h"

namespace angel {

static const char *ev2str(int events)
{
    switch (events) {
    case Read: return "<Read>";
    case Write: return "<Write>";
    case Error: return "<Error>";
    case Read | Write: return "<Read|Write>";
    case Read | Error: return "<Read|Error>";
    case Write | Error: return "<Write|Error>";
    default: return "<None>";
    }
}

channel::channel(evloop *loop, int fd, bool hold_fd)
    : loop(loop), evfd(fd), hold_fd(hold_fd), filter(0), trigger(0)
{
    Assert(loop);
    Assert(fd >= 0);
}

channel::~channel()
{
    if (hold_fd) {
        close(evfd);
        log_debug("~channel(): close(fd=%d)", evfd);
    }
}

void channel::add()
{
    loop->run_in_loop([this]{
            log_debug("channel(fd=%d) is added to the loop(%p)", evfd, loop);
            auto r = loop->channel_map.emplace(evfd, this);
            // MUST NOT add duplicate channel.
            Assert(r.second);
            enable_read();
            });
}

void channel::remove()
{
    loop->run_in_loop([this]{ disable_all(); });
    // We can't directly delete channel in the current loop thread,
    // which will make the channel in channel->handle_event() a hanging pointer,
    // and instead use queue_in_loop().
    loop->queue_in_loop([this]{
            log_debug("channel(fd=%d) is removed from the loop(%p)", evfd, loop);
            loop->channel_map.erase(evfd);
            });
}

void channel::enable_read()
{
    if (!is_reading()) {
        filter |= Read;
        loop->dispatcher->add(evfd, Read);
        log_debug("channel(fd=%d) enable <Read>", evfd);
    }
}

void channel::disable_read()
{
    if (is_reading()) {
        filter &= ~Read;
        loop->dispatcher->remove(evfd, Read);
        log_debug("channel(fd=%d) disable <Read>", evfd);
    }
}

void channel::enable_write()
{
    if (!is_writing()) {
        filter |= Write;
        loop->dispatcher->add(evfd, Write);
        log_debug("channel(fd=%d) enable <Write>", evfd);
    }
}

void channel::disable_write()
{
    if (is_writing()) {
        filter &= ~Write;
        loop->dispatcher->remove(evfd, Write);
        log_debug("channel(fd=%d) disable <Write>", evfd);
    }
}

void channel::disable_all()
{
    if (filter) {
        loop->dispatcher->remove(evfd, filter);
        log_debug("channel(fd=%d) disable %s", evfd, ev2str(filter));
        read_handler  = nullptr;
        write_handler = nullptr;
        error_handler = nullptr;
        filter = 0;
    }
}

void channel::handle_event()
{
    if (!trigger) return;
    log_debug("channel(fd=%d) triggered event %s", fd(), ev2str(trigger));
    if (trigger & Error)
        if (error_handler) error_handler();
    if (trigger & Read)
        if (read_handler) read_handler();
    if (trigger & Write)
        if (write_handler) write_handler();
    trigger = 0;
}

void channel::set_read_handler(event_handler_t handler)
{
    read_handler = std::move(handler);
}

void channel::set_write_handler(event_handler_t handler)
{
    write_handler = std::move(handler);
}

void channel::set_error_handler(event_handler_t handler)
{
    error_handler = std::move(handler);
}

}
