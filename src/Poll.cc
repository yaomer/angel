#include "config.h"

#ifdef _ANGEL_HAVE_POLL

#include <sys/poll.h>
#include <unistd.h>
#include <vector>
#include "Poll.h"
#include "EventLoop.h"
#include "Channel.h"

using namespace Angel;

void Poll::add(int fd, int events)
{
    struct pollfd ev;
    evset(ev, fd, events);
    _pollfds.push_back(ev);
    auto it = std::pair<int, int>(ev.fd, _pollfds.size() - 1);
    _indexs.insert(it);
}

void Poll::change(int fd, int events)
{
    auto it = _indexs.find(fd);
    struct pollfd *pfd = &_pollfds[it->second];
    evset(*pfd, fd, events);
}

void Poll::remove(int fd)
{
    auto it = _indexs.find(fd);
    size_t end = _pollfds.size() - 1;
    std::swap(_pollfds[it->second], _pollfds[end]);
    _pollfds.pop_back();
    _indexs.erase(fd);
}

int Poll::wait(EventLoop *loop, int64_t timeout)
{
    int nevents = poll(&_pollfds[0], _pollfds.size(), timeout);
    int ret = nevents;
    if (nevents > 0) {
        for (auto& it : _pollfds) {
            if (it.revents > 0) {
                auto chl = loop->searchChannel(it.fd);
                chl->setRevents(evret(it.revents));
                loop->fillActiveChannel(chl);
                if (--nevents == 0)
                    break;
            }
        }
    }
    return ret;
}

void Poll::evset(struct pollfd& ev, int fd, int events)
{
    ev.fd = fd;
    ev.events = 0;
    ev.revents = 0;
    if (events & Channel::READ)
        ev.events |= POLLIN;
    if (events & Channel::WRITE)
        ev.events |= POLLOUT;
}

int Poll::evret(int events)
{
    int rets = 0;
    if (events & POLLIN)
        rets |= Channel::READ;
    if (events & POLLOUT)
        rets |= Channel::WRITE;
    if (events & POLLERR)
        rets |= Channel::ERROR;
    return rets;
}

#endif // _ANGEL_HAVE_POLL
