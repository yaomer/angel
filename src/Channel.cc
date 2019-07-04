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
    LOG_INFO << "[Channel::ctor]";
}

Channel::~Channel()
{
    LOG_INFO << "[Channel::dtor]";
}

void Channel::enableRead()
{
    int evbef = _events;
    _events |= READ;
    LOG_DEBUG << "[fd:" << fd() << "]" << " events is [" << eventStr[evbef]
              << " -> " << eventStr[_events] << "]";
}

void Channel::enableWrite()
{
    int evbef = _events;
    _events |= WRITE;
    changeEvents();
    LOG_DEBUG << "[fd:" << fd() << "]" << " events is [" << eventStr[evbef]
              << " -> " << eventStr[_events] << "]";
}

void Channel::disableWrite()
{
    int evbef = _events;
    _events &= ~WRITE;
    changeEvents();
    LOG_DEBUG << "[fd:" << fd() << "]" << " events is [" << eventStr[evbef]
              << " -> " << eventStr[_events] << "]";
}

void Channel::changeEvents()
{
    _loop->changeEvent(fd(), events());
}

void Channel::handleEvent()
{
    LOG_DEBUG << "[fd:" << fd() << "]" << " revents is ["
              << eventStr[revents()] << "]";
    if (revents() & ERROR)
        if (_errorCb) _errorCb();
    if (revents() & READ)
        if (_readCb) _readCb();
    if (revents() & WRITE)
        if (_writeCb) _writeCb();
}
