#include "config.h"

#ifdef _ANGEL_HAVE_EPOLL

#include <sys/epoll.h>
#include <vector>
#include "EventLoop.h"
#include "Channel.h"
#include "Epoll.h"
#include "LogStream.h"

using namespace Angel;

Epoll::Epoll()
    : _addFds(0),
    _evListSize(_INIT_EVLIST_SIZE)
{
    _epfd = epoll_create(1);
    _evList.reserve(_evListSize);
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
        LOG_ERROR << "[epoll_ctl -> EPOLL_CTL_ADD]: " << strerrno();
    if (++_addFds >= _evListSize) {
        _evListSize *= 2;
        _evList.reserve(_evListSize);
    }
}

void Epoll::change(int fd, int events)
{
    struct epoll_event ev;
    evset(ev, fd, events);
    if (epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
        LOG_ERROR << "[epoll_ctl -> EPOLL_CTL_MOD]: " << strerrno();
}

void Epoll::remove(int fd, int events)
{
    if (epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL) < 0)
        LOG_ERROR << "[epoll_ctl -> EPOLL_CTL_DEL]: " << strerrno();
    _addFds--;
}

int Epoll::wait(EventLoop *loop, int64_t timeout)
{
    int nevents = epoll_wait(_epfd, &_evList[0], _evList.capacity(), timeout);
    if (nevents > 0) {
        for (int i = 0; i < nevents; i++) {
            auto chl = loop->search(_evList[i].data.fd);
            chl->setRevents(evret(_evList[i].events));
            loop->fillActiveChannel(chl);
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            LOG_ERROR << "epoll_wait(): " << strerrno();
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
