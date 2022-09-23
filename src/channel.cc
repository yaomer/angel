#include <angel/channel.h>

#include <angel/evloop.h>
#include <angel/logger.h>

namespace angel {

channel::channel(evloop *loop)
    : loop(loop), evfd(-1), filter(0), trigger(0)
{
}

channel::~channel() = default;

void channel::enable_read()
{
    filter |= Read;
}

void channel::enable_write()
{
    filter |= Write;
    loop->change_event(evfd, filter);
}

void channel::disable_write()
{
    filter &= ~Write;
    loop->change_event(evfd, filter);
}

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

void channel::handle_event()
{
    log_debug("channel(fd=%d) triggered event is %s", fd(), ev2str(trigger));
    if (trigger & Error)
        if (error_handler) error_handler();
    if (trigger & Read)
        if (read_handler) read_handler();
    if (trigger & Write)
        if (write_handler) write_handler();
}

}
