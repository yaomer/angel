#include "config.h"

#ifdef ANGEL_HAVE_KQUEUE

#include "kqueue.h"

#include <unistd.h>

#include <angel/evloop.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

kqueue_base_t::kqueue_base_t()
{
    kqfd = kqueue();
    evlist.resize(EVLIST_INIT_SIZE);
    evmap.resize(EVLIST_INIT_SIZE);
    set_name("kqueue");
}

kqueue_base_t::~kqueue_base_t()
{
    ::close(kqfd);
}

// Adds the events to the kqueue.
//
// Re-adding an existing event will modify the parameters of the original event,
// and not result in a duplicate entry.
//
// Adding an event automatically enables it, unless overridden by
// the EV_DISABLE flag.
void kqueue_base_t::add(int fd, int events)
{
    struct kevent kev;
    if (events & Read) {
        EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("kevent: %s", strerrno());
    }
    if (events & Write) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("kevent: %s", strerrno());
    }
    resize_if(fd, evmap);
    if (!evmap[fd]) resize_if(++added_fds, evlist);
    evmap[fd] |= events;
}

// Removes all events registered on the fd,
// and kqueue will remove the fd from the kernel event list.
void kqueue_base_t::remove(int fd, int events)
{
    struct kevent kev;
    if (events & Read) {
        EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("kevent: %s", strerrno());
    }
    if (events & Write) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        if (kevent(kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            log_error("kevent: %s", strerrno());
    }
    evmap[fd] &= ~events;
    if (!evmap[fd]) added_fds--;
}

static int evret(struct kevent& ev)
{
    int revs = 0;
    if (ev.filter == EVFILT_READ) {
        revs = Read;
    } else if (ev.filter == EVFILT_WRITE) {
        revs = Write;
    } else if (ev.flags == EV_ERROR) {
        errno = ev.data;
        revs = Error;
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
    } else if (nevents < 0) {
        if (errno != EINTR)
            log_error("kevent: %s", strerrno());
    }
    return nevents;
}

}

#endif
