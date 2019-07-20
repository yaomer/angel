#include <unistd.h>
#include <errno.h>
#include <string>
#include "Channel.h"
#include "EventLoop.h"
#include "LogStream.h"

using namespace Angel;

namespace Angel {

const char *eventStr[] = {
    "None",
    "READ",
    "WRITE",
    "READ|WRITE",
    "ERROR",
};
}

Channel::Channel(EventLoop *loop)
    : _loop(loop),
    _fd(-1),
    _events(0),
    _revents(0)
{
    logInfo("[Channel::ctor]");
}

Channel::~Channel()
{
    logInfo("[Channel::dtor]");
}

const char *Channel::evstr()
{
    return eventStr[_events];
}

const char *Channel::revstr()
{
    return eventStr[_revents];
}

void Channel::enableRead()
{
    _events |= READ;
    logInfo("[fd = %d] enable READ", fd());
}

void Channel::enableWrite()
{
    _events |= WRITE;
    changeEvents();
    logInfo("[fd = %d] enable WRITE", fd());
}

void Channel::disableWrite()
{
    _events &= ~WRITE;
    changeEvents();
    logInfo("[fd = %d] disable WRITE", fd());
}

void Channel::changeEvents()
{
    _loop->changeEvent(fd(), events());
}

// 事件多路分发器，如果Channel上有事件发生，则根据
// 不同的事件触发不同的回调
void Channel::handleEvent()
{
    logInfo("[fd = %d] revents is [%s]", fd(), revstr());
    if (revents() & ERROR)
        if (_errorCb) _errorCb();
    if (revents() & READ)
        if (_readCb) _readCb();
    if (revents() & WRITE)
        if (_writeCb) _writeCb();
}
