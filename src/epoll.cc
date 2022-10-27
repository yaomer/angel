#include <angel/config.h>

#ifdef ANGEL_HAVE_EPOLL

#include "epoll.h"

#include <unistd.h>

#include <angel/evloop.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

epoll_base_t::epoll_base_t()
{
    epfd = epoll_create(1);
    evlist.resize(EVLIST_INIT_SIZE);
    evmap.resize(EVLIST_INIT_SIZE);
    set_name("epoll");
}

epoll_base_t::~epoll_base_t()
{
    close(epfd);
}

#define EEV_SET(eev, efd, filter) do { \
    (eev).data.fd = (efd); \
    if ((filter) & Read) (eev).events |= EPOLLIN; \
    if ((filter) & Write) (eev).events |= EPOLLOUT; \
} while (0)


void epoll_base_t::add(int fd, int events)
{
    resize_if(fd, evmap);

    auto& filter = evmap[fd];
    int op = filter ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    if (!filter) resize_if(++added_fds, evlist);
    filter |= events;

    struct epoll_event eev;
    EEV_SET(eev, fd, filter);

    if (epoll_ctl(epfd, op, fd, &eev) < 0)
        log_error("epoll_ctl: %s", strerrno());
}

void epoll_base_t::remove(int fd, int events)
{
    struct epoll_event eev;
    struct epoll_event *eevp = nullptr;

    auto& filter = evmap[fd];
    filter &= ~events;

    int op = filter ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

    if (filter) {
        EEV_SET(eev, fd, filter);
        eevp = &eev;
    } else {
        added_fds--;
    }

    if (epoll_ctl(epfd, op, fd, eevp) < 0)
        log_error("epoll_ctl: %s", strerrno());
}

static int evret(int events)
{
    int revs = 0;
    if (events & EPOLLIN) revs |= Read;
    if (events & EPOLLOUT) revs |= Write;
    if (events & EPOLLERR) revs |= Error;
    return revs;
}

int epoll_base_t::wait(evloop *loop, int64_t timeout)
{
    int nevents = epoll_wait(epfd, &evlist[0], evlist.size(), timeout);
    if (nevents > 0) {
        for (int i = 0; i < nevents; i++) {
            auto chl = loop->search_channel(evlist[i].data.fd);
            chl->set_trigger_events(evret(evlist[i].events));
            loop->active_channels.emplace_back(chl);
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            log_error("epoll_wait: %s", strerrno());
    }
    return nevents;
}

}

#endif
