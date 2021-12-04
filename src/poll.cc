#include "config.h"

#ifdef ANGEL_HAVE_POLL

#include <unistd.h>

#include "poll.h"
#include "evloop.h"
#include "logger.h"
#include "util.h"

namespace angel {

using namespace util;

poll_base_t::poll_base_t()
{
    set_name("poll");
}

poll_base_t::~poll_base_t() = default;

static inline void evset(struct pollfd& ev, int fd, int events)
{
    ev.fd = fd;
    ev.events = 0;
    ev.revents = 0;
    if (events & Read)
        ev.events |= POLLIN;
    if (events & Write)
        ev.events |= POLLOUT;
}

void poll_base_t::add(int fd, int events)
{
    struct pollfd ev;
    evset(ev, fd, events);
    poll_fds.emplace_back(ev);
    indexs.emplace(ev.fd, poll_fds.size() - 1);
}

void poll_base_t::change(int fd, int events)
{
    auto it = indexs.find(fd);
    struct pollfd *pfd = &poll_fds[it->second];
    evset(*pfd, fd, events);
}

void poll_base_t::remove(int fd, int events)
{
    auto it = indexs.find(fd);
    size_t end = poll_fds.size() - 1;
    std::swap(poll_fds[it->second], poll_fds[end]);
    poll_fds.pop_back();
    indexs.erase(fd);
}

static int evret(int events)
{
    int revs = 0;
    if (events & POLLIN) revs |= Read;
    if (events & POLLOUT) revs |= Write;
    if (events & POLLERR) revs |= Error;
    return revs;
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
