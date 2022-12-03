#include <angel/config.h>

#ifdef ANGEL_HAVE_POLL

#include "poll.h"

#include <angel/evloop.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

poll_base_t::poll_base_t()
{
    set_name("poll");
}

poll_base_t::~poll_base_t()
{
}

#define EVMAP(poll_events, events) \
    if ((events) & Read) (poll_events) |= POLLIN; \
    if ((events) & Write) (poll_events) |= POLLOUT

void poll_base_t::add(int fd, int events)
{
    int poll_events = 0;

    EVMAP(poll_events, events);

    auto it = indexs.find(fd);
    if (it == indexs.end()) { // Add a new fd.
        struct pollfd pfd;
        pfd.fd      = fd;
        pfd.events  = poll_events;
        pfd.revents = 0;
        pollfds.emplace_back(pfd);
        indexs.emplace(fd, pollfds.size() - 1);
    } else { // Add a new event for fd.
        pollfds[it->second].events |= poll_events;
    }
}

void poll_base_t::remove(int fd, int events)
{
    int poll_events = 0;

    EVMAP(poll_events, events);

    auto it = indexs.find(fd);
    if (it == indexs.end()) {
        log_warn("remove(): fd(%d) does not exist", fd);
        return;
    }

    auto& pfd = pollfds[it->second];
    pfd.events &= ~poll_events;

    if (!pfd.events) {
        auto& back = pollfds.back();
        indexs[back.fd] = it->second;
        indexs.erase(it);

        std::swap(pfd, back);
        pollfds.pop_back();
    }
}

static int evret(int revents)
{
    int revs = 0;
    if (revents == (POLLIN | POLLHUP)) {
        // Non-blocking connect(2) failed.
        revs = Read | Write;
    } else {
        if (revents & POLLIN) revs |= Read;
        if (revents & POLLOUT) revs |= Write;
        if (revents & POLLERR) revs |= Error;
    }
    return revs;
}

int poll_base_t::wait(evloop *loop, int64_t timeout)
{
    int nevents = poll(&pollfds[0], pollfds.size(), timeout);
    int retval = nevents;
    if (nevents > 0) {
        for (auto& pfd : pollfds) {
            if (pfd.revents > 0) {
                auto chl = loop->search_channel(pfd.fd);
                chl->trigger = evret(pfd.revents);
                loop->active_channels.emplace_back(chl);
                if (--nevents == 0)
                    break;
            }
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            log_error("poll: %s", strerrno());
    }
    return retval;
}

}

#endif
