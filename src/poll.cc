#include <angel/config.h>

#ifdef ANGEL_HAVE_POLL

#include "poll.h"

#include <unistd.h>

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

void poll_base_t::add(int fd, int events)
{
    struct pollfd pfd{0};
    pfd.fd = fd;
    if (events & Read) pfd.events |= POLLIN;
    if (events & Write) pfd.events |= POLLOUT;

    auto it = indexs.find(fd);
    if (it == indexs.end()) {
        pollfds.emplace_back(pfd);
        indexs.emplace(fd, pollfds.size() - 1);
    } else {
        pollfds[it->second].events |= pfd.events;
    }
}

void poll_base_t::remove(int fd, int events)
{
    int poll_events = 0;
    if (events & Read) poll_events |= POLLIN;
    if (events & Write) poll_events |= POLLOUT;

    auto& pfd = pollfds[indexs[fd]];
    pfd.events &= ~poll_events;

    if (!pfd.events) {
        std::swap(pfd, pollfds.back());
        pollfds.pop_back();
        indexs.erase(fd);
    }
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
    int nevents = poll(&pollfds[0], pollfds.size(), timeout);
    int retval = nevents;
    if (nevents > 0) {
        for (auto& pfd : pollfds) {
            if (pfd.revents > 0) {
                auto chl = loop->search_channel(pfd.fd);
                chl->set_trigger_events(evret(pfd.revents));
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
