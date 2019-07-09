#include "config.h"

#ifdef _ANGEL_HAVE_KQUEUE

#include <sys/event.h>
#include <unistd.h>
#include <vector>
#include "Kqueue.h"
#include "EventLoop.h"
#include "Channel.h"
#include "LogStream.h"

using namespace Angel;

Kqueue::Kqueue()
    : _addFds(0),
    _evlistSize(_INIT_EVLIST_SIZE)
{
    _kqfd = kqueue();
    _evlist.reserve(_evlistSize);
}

Kqueue::~Kqueue()
{
    close(_kqfd);
}

void Kqueue::add(int fd, int events)
{
    change(fd, events);
    if (++_addFds >= _evlistSize) {
        _evlistSize *= 2;
        _evlist.reserve(_evlistSize);
    }
}

void Kqueue::change(int fd, int events)
{
    struct kevent kev;
    if (events & Channel::READ) {
        EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (kevent(_kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            LOG_ERROR << "[kevent -> EV_ADD]: " << strerrno();
    }
    if (events & Channel::WRITE) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
        if (kevent(_kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            LOG_ERROR << "[kevent -> EV_ADD]: " << strerrno();
    }
}

void Kqueue::remove(int fd, int events)
{
    struct kevent kev;
    // 删除fd上注册的所有事件，kqueue即会从内核事件列表中移除fd
    if (events & Channel::READ) {
        EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        if (kevent(_kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            LOG_ERROR << "[kevent -> EV_DELETE]: " << strerrno();
    }
    if (events & Channel::WRITE) {
        EV_SET(&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        if (kevent(_kqfd, &kev, 1, nullptr, 0, nullptr) < 0)
            LOG_ERROR << "[kevent -> EV_DELETE]: " << strerrno();
    }
    _addFds--;
}

int Kqueue::wait(EventLoop *loop, int64_t timeout)
{
    struct timespec tsp;

    if (timeout > 0) {
        tsp.tv_sec = timeout / 1000;
        tsp.tv_nsec = (timeout % 1000) * 1000 * 1000;
    }
    int nevents = kevent(_kqfd, nullptr, 0, &_evlist[0], _evlist.capacity(),
            timeout > 0 ? &tsp : nullptr);

    if (nevents > 0) {
        for (int i = 0; i < nevents; i++) {
            auto chl = loop->searchChannel(_evlist[i].ident);
            chl->setRevents(evret(_evlist[i]));
            loop->fillActiveChannel(chl);
        }
    }
    return nevents;
}

int Kqueue::evret(struct kevent& ev)
{
    int rets = 0;
    if (ev.filter == EVFILT_READ)
        rets = Channel::READ;
    else if (ev.filter == EVFILT_WRITE)
        rets = Channel::WRITE;
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
            rets = Channel::ERROR;
            break;
        }
    }
    return rets;
}

#endif // _ANGEL_HAVE_KQUEUE
