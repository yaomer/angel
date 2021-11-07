#include "config.h"

#ifdef ANGEL_HAVE_POLL

#include <unistd.h>

#include "poll.h"
#include "evloop.h"
#include "logger.h"

namespace angel {

using namespace util;

static inline void evset(struct pollfd& ev, int fd, event events)
{
    ev.fd = fd;
    ev.events = 0;
    ev.revents = 0;
    if (is_enum_true(events & event::read))
        ev.events |= POLLIN;
    if (is_enum_true(events & event::write))
        ev.events |= POLLOUT;
}

void poll_base_t::add(int fd, event events)
{
    struct pollfd ev;
    evset(ev, fd, events);
    poll_fds.emplace_back(ev);
    indexs.emplace(ev.fd, poll_fds.size() - 1);
}

void poll_base_t::change(int fd, event events)
{
    auto it = indexs.find(fd);
    struct pollfd *pfd = &poll_fds[it->second];
    evset(*pfd, fd, events);
}

void poll_base_t::remove(int fd, event events)
{
    auto it = indexs.find(fd);
    size_t end = poll_fds.size() - 1;
    std::swap(poll_fds[it->second], poll_fds[end]);
    poll_fds.pop_back();
    indexs.erase(fd);
}

static event evret(int events)
{
    event rets{};
    if (events & POLLIN)
        rets |= event::read;
    if (events & POLLOUT)
        rets |= event::write;
    if (events & POLLERR)
        rets |= event::error;
    return rets;
}

int poll_base_t::wait(evloop *loop, int64_t timeout)
{
    int nevents = poll(&poll_fds[0], poll_fds.size(), timeout);
    int ret = nevents;
    if (nevents > 0) {
        for (auto& it : poll_fds) {
            if (it.revents > 0) {
                auto chl = loop->search_channel(it.fd);
                chl->set_trigger_events(evret(it.revents));
                loop->fill_active_channel(chl);
                if (--nevents == 0)
                    break;
            }
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            log_error("poll: %s", strerrno());
    }
    return ret;
}

}

#endif
