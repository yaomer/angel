#include "config.h"

#ifdef _ANGEL_HAVE_EPOLL

#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include "EventLoop.h"
#include "Channel.h"
#include "Epoll.h"
#include "LogStream.h"

using namespace Angel;

Epoll::Epoll()
    : _addFds(0),
    _evListSize(evlist_init_size)
{
    _epfd = epoll_create(1);
    _evList.resize(_evListSize);
    setName("epoll");
}

Epoll::~Epoll()
{
    close(_epfd);
}

void Epoll::add(int fd, int events)
{
    struct epoll_event ev;
    evset(ev, fd, events);
    if (epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
        logError("[epoll_ctl -> EPOLL_CTL_ADD]: %s", strerrno());
    if (++_addFds >= _evListSize) {
        _evListSize *= 2;
        _evList.resize(_evListSize);
    }
}

void Epoll::change(int fd, int events)
{
    struct epoll_event ev;
    evset(ev, fd, events);
    if (epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
        logError("[epoll_ctl -> EPOLL_CTL_MOD]: %s", strerrno());
}

void Epoll::remove(int fd, int events)
{
    if (epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL) < 0)
        logError("[epoll_ctl -> EPOLL_CTL_DEL]: %s", strerrno());
    _addFds--;
}

int Epoll::wait(EventLoop *loop, int64_t timeout)
{
    int nevents = epoll_wait(_epfd, &_evList[0], _evList.size(), timeout);
    if (nevents > 0) {
        for (int i = 0; i < nevents; i++) {
            auto chl = loop->searchChannel(_evList[i].data.fd);
            chl->setRevents(evret(_evList[i].events));
            loop->fillActiveChannel(chl);
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            logError("epoll_wait: %s", strerrno());
    }
    return nevents;
}

void Epoll::evset(struct epoll_event& ev, int fd, int events)
{
    ev.data.fd = fd;
    ev.events = 0;
    if (events & Channel::READ)
        ev.events |= EPOLLIN;
    if (events & Channel::WRITE)
        ev.events |= EPOLLOUT;
}

int Epoll::evret(int events)
{
    int rets = 0;
    if (events & EPOLLIN)
        rets |= Channel::READ;
    if (events & EPOLLOUT)
        rets |= Channel::WRITE;
    if (events & EPOLLERR)
        rets |= Channel::ERROR;
    return rets;
}

#endif
