#include "config.h"

#ifdef ANGEL_HAVE_EPOLL

#include <unistd.h>
#include <sys/epoll.h>
#include <vector>

#include "evloop.h"
#include "epoll.h"
#include "logger.h"

using namespace angel;
using namespace angel::util;

epoll_base_t::epoll_base_t()
    : added_fds(0)
{
    epfd = epoll_create(1);
    evlist.resize(evlist_init_size);
    set_name("epoll");
}

epoll_base_t::~epoll_base_t()
{
    close(epfd);
}

static inline void evset(struct epoll_event& ev, int fd, event events)
{
    ev.data.fd = fd;
    ev.events = 0;
    if (is_enum_true(events & event::read))
        ev.events |= EPOLLIN;
    if (is_enum_true(events & event::write))
        ev.events |= EPOLLOUT;
}

void epoll_base_t::add(int fd, event events)
{
    struct epoll_event ev;
    evset(ev, fd, events);
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
        log_error("[epoll_ctl -> EPOLL_CTL_ADD]: %s", strerrno());
    if (++added_fds >= evlist.size()) {
        evlist.resize(evlist.size() * 2);
    }
}

void epoll_base_t::change(int fd, event events)
{
    struct epoll_event ev;
    evset(ev, fd, events);
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
        log_error("[epoll_ctl -> EPOLL_CTL_MOD]: %s", strerrno());
}

void epoll_base_t::remove(int fd, event events)
{
    UNUSED(events);
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) < 0)
        log_error("[epoll_ctl -> EPOLL_CTL_DEL]: %s", strerrno());
    added_fds--;
}

static event evret(int events)
{
    event rets{};
    if (events & EPOLLIN)
        rets |= event::read;
    if (events & EPOLLOUT)
        rets |= event::write;
    if (events & EPOLLERR)
        rets |= event::error;
    return rets;
}

int epoll_base_t::wait(evloop *loop, int64_t timeout)
{
    int nevents = epoll_wait(epfd, &evlist[0], evlist.size(), timeout);
    if (nevents > 0) {
        for (int i = 0; i < nevents; i++) {
            auto chl = loop->search_channel(evlist[i].data.fd);
            chl->set_trigger_events(evret(evlist[i].events));
            loop->fill_active_channel(chl);
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            log_error("epoll_wait: %s", strerrno());
    }
    return nevents;
}

#endif
