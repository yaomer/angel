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
    if (events == 0)
        events = Channel::READ;
    change(fd, events);
    if (++_addFds >= _evlistSize) {
        _evlistSize *= 2;
        _evlist.reserve(_evlistSize);
    }
}

void Kqueue::change(int fd, int events)
{
    struct kevent ev[2];
    int evNums = 0;

    if (events == 0)
        return;
    if (events & Channel::READ) {
        EV_SET(&ev[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, nullptr);
        evNums++;
    }
    if (events & Channel::WRITE) {
        EV_SET(&ev[evNums], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, nullptr);
        evNums++;
    }
    if (kevent(_kqfd, ev, evNums, nullptr, 0, nullptr) < 0)
        LOG_ERROR << "kevent: " << strerrno();
}

void Kqueue::remove(int fd)
{
    struct kevent ev[2];
    // 删除fd上注册的所有事件，kqueue即会从内核事件列表中移除fd
    change(fd, Channel::READ | Channel::WRITE);
    EV_SET(&ev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&ev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    if (kevent(_kqfd, ev, 2, nullptr, 0, nullptr) < 0)
        LOG_ERROR << "kevent: " << strerrno();
    _addFds--;
}

int Kqueue::wait(EventLoop *loop, int64_t timeout)
{
    struct timespec tsp;
    int nevents;

    if (timeout > 0) {
        tsp.tv_sec = timeout / 1000;
        tsp.tv_nsec = (timeout % 1000) * 1000 * 1000;
        nevents = kevent(_kqfd, nullptr, 0, &_evlist[0], _evlist.capacity(), &tsp);
    } else {
        // 此时kqueue将会一直阻塞直到有事件发生
        // 如果tsp.tv_sec = 0 && tsp.tv_nsec = 0则kqueue会立刻返回
        nevents = kevent(_kqfd, nullptr, 0, &_evlist[0], _evlist.capacity(), nullptr);
    }

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
