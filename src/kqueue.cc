#include "config.h"

#ifdef ANGEL_HAVE_KQUEUE

#include <unistd.h>
#include <string.h>

#include "kqueue.h"
#include "evloop.h"
#include "logger.h"

namespace angel {

using namespace util;

kqueue_base_t::kqueue_base_t()
    : added_fds(0)
{
    kqfd = kqueue();
    evlist.resize(evlist_init_size);
    set_name("kqueue");
}

kqueue_base_t::~kqueue_base_t()
{
    ::close(kqfd);
}

void kqueue_base_t::add(int fd, event events)
{
    change(fd, events);
    if (++added_fds >= evlist.size()) {
        evlist.resize(evlist.size() * 2);
    }
}

void kqueue_base_t::change(int fd, event events)
{
    struct kevent kev;
    if (is_enum_true(events & event::read)) {
        EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_ADD]: %s", strerrno());
    }
    if (is_enum_true(events & event::write)) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_ADD]: %s", strerrno());
    }
}

void kqueue_base_t::remove(int fd, event events)
{
    struct kevent kev;
    // 删除fd上注册的所有事件，kqueue即会从内核事件列表中移除fd
    if (is_enum_true(events & event::read)) {
        EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_DELETE]: %s", strerrno());
    }
    if (is_enum_true(events & event::write)) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_DELETE]: %s", strerrno());
    }
    added_fds--;
}

static event evret(struct kevent& ev)
{
    event events{};
    if (ev.filter == EVFILT_READ)
        events = event::read;
    else if (ev.filter == EVFILT_WRITE)
        events = event::write;
    else if (ev.flags) {
        switch (ev.flags) {
        case ENOENT:
        case EINVAL:
        case EPIPE:
        case EPERM:
        case EBADF:
            break;
        default:
            errno = ev.data;
            events = event::error;
            break;
        }
    }
    return events;
}

int kqueue_base_t::wait(evloop *loop, int64_t timeout)
{
    struct timespec tsp;
    memset(&tsp, 0, sizeof(tsp));

    if (timeout > 0) {
        tsp.tv_sec = timeout / 1000;
        tsp.tv_nsec = (timeout % 1000) * 1000 * 1000;
    }
    int nevents = kevent(kqfd, nullptr, 0, &evlist[0], evlist.size(),
            timeout >= 0 ? &tsp : nullptr);

    if (nevents > 0) {
        for (int i = 0; i < nevents; i++) {
            auto chl = loop->search_channel(evlist[i].ident);
            chl->set_trigger_events(evret(evlist[i]));
            loop->fill_active_channel(chl);
        }
    }
    return nevents;
}

}

#endif
