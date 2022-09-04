#include "config.h"

#ifdef ANGEL_HAVE_KQUEUE

#include <unistd.h>
#include <string.h>

#include "kqueue.h"
#include "evloop.h"
#include "logger.h"
#include "util.h"

namespace angel {

using namespace util;

static const size_t evlist_init_size = 64;

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

void kqueue_base_t::add(int fd, int events)
{
    change(fd, events);
    if (++added_fds >= evlist.size()) {
        evlist.resize(evlist.size() * 2);
    }
}

void kqueue_base_t::change(int fd, int events)
{
    struct kevent kev;
    if (events & Read) {
        EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_ADD]: %s", strerrno());
    }
    if (events & Write) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_ADD]: %s", strerrno());
    }
}

void kqueue_base_t::remove(int fd, int events)
{
    struct kevent kev;
    // 删除fd上注册的所有事件，kqueue即会从内核事件列表中移除fd
    if (events & Read) {
        EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_DELETE]: %s", strerrno());
    }
    if (events & Write) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("[kevent -> EV_DELETE]: %s", strerrno());
    }
    added_fds--;
}

static int evret(struct kevent& ev)
{
    int revs = 0;
    if (ev.filter == EVFILT_READ)
        revs = Read;
    else if (ev.filter == EVFILT_WRITE)
        revs = Write;
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
            revs = Error;
            break;
        }
    }
    return revs;
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
            loop->active_channels.emplace_back(chl);
        }
    }
    return nevents;
}

}

#endif
