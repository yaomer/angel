#include "config.h"

#ifdef _ANGEL_HAVE_SELECT

#include <sys/select.h>
#include "EventLoop.h"
#include "Select.h"
#include "LogStream.h"

using namespace Angel;

Select::Select()
{
    FD_ZERO(&_rdset);
    FD_ZERO(&_wrset);
    _fds.reserve(_INIT_FDS_SIZE);
    _maxFd = -1;
}

Select::~Select()
{

}

#define MAX(a, b) ((a) > (b) ? (a) : (b))

void Select::add(int fd, int events)
{
    _fds.push_back(fd);
    auto it = std::pair<int, int>(fd, _fds.size() - 1);
    _indexs.insert(it);
    change(fd, events);
    _maxFd = MAX(_maxFd, fd);
}

void Select::change(int fd, int events)
{
    // 要修改关联到fd上的事件，必须先从_rdset和_wrset中
    // 清除掉原先注册的fd，然后再重新注册，不然就可能会
    // 出现重复注册同一个fd
    FD_CLR(fd, &_rdset);
    FD_CLR(fd, &_wrset);
    if (events & Channel::READ)
        FD_SET(fd, &_rdset);
    if (events & Channel::WRITE)
        FD_SET(fd, &_wrset);
}

void Select::remove(int fd)
{
    auto it = _indexs.find(fd);
    size_t end = _fds.size() - 1;
    std::swap(_fds[it->second], _fds[end]);
    _fds.pop_back();
    _indexs.erase(fd);
    change(fd, 0);
    if (fd == _maxFd)
        _maxFd--;
}

int Select::wait(EventLoop *loop, int64_t timeout)
{
    struct timeval tv;
    // 我们不能直接将_rdset和_wrset传递给select()，因为select()会改写它们
    // 以存放活跃的fd，我们需要传递给它一个副本
    fd_set rdset, wrset, errset;
    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    FD_ZERO(&errset);
    rdset = _rdset;
    wrset = _wrset;
    if (timeout > 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
    }
    int nevents = select(_maxFd + 1, &rdset, &wrset, &errset,
            timeout > 0 ? &tv : nullptr);
    int rets = nevents;
    if (nevents > 0) {
        for (auto& it : _fds) {
            int revs = 0;
            if (FD_ISSET(it, &rdset))
                revs |= Channel::READ;
            if (FD_ISSET(it, &wrset))
                revs |= Channel::WRITE;
            if (FD_ISSET(it, &errset))
                revs |= Channel::ERROR;
            auto chl = loop->searchChannel(it);
            chl->setRevents(revs);
            loop->fillActiveChannel(chl);
            if (revs > 0 && --nevents == 0)
                break;
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            LOG_ERROR << "select(): " << strerrno();
    }
    return rets;
}

#endif
