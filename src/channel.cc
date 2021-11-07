#include "channel.h"
#include "evloop.h"
#include "logger.h"

namespace angel {

channel::channel(evloop *loop)
    : loop(loop),
    evfd(-1),
    filter(),
    trigger()
{
}

void channel::change_events()
{
    log_debug("fd=%d events is %s", fd(), ev2str(filter));
    loop->change_event(evfd, filter);
}

// 事件多路分发器，如果channel上有事件发生，则根据不同的事件触发不同的回调
void channel::handle_event()
{
    log_debug("fd=%d revents is %s", fd(), ev2str(trigger));
    if (is_enum_true(trigger & event::error))
        if (error_handler) error_handler();
    if (is_enum_true(trigger & event::read))
        if (read_handler) read_handler();
    if (is_enum_true(trigger & event::write))
        if (write_handler) write_handler();
}

static constexpr event rw_event = event::read | event::write;
static constexpr event re_event = event::read | event::error;
static constexpr event we_event = event::write | event::error;

const char *channel::ev2str(event events)
{
    switch (events) {
    case event::read: return "EV_READ";
    case event::write: return "EV_WRITE";
    case event::error: return "EV_ERROR";
    case rw_event: return "EV_READ|EV_WRITE";
    case re_event: return "EV_READ|EV_ERROR";
    case we_event: return "EV_WRITE|EV_ERROR";
    default: return "NONE";
    }
}

}
