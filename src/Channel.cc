#include <unistd.h>
#include <errno.h>
#include <string>

#include "Channel.h"
#include "EventLoop.h"
#include "LogStream.h"

using namespace Angel;

const char *Channel::ev2str(int events)
{
    switch (events) {
    case READ: return "READ";
    case WRITE: return "WRITE";
    case READ | WRITE: return "READ|WRITE";
    case ERROR: return "ERROR";
    default: return "NONE";
    }
}

void Channel::changeEvents()
{
    logDebug("fd = %d events is [%s]", fd(), evstr());
    _loop->changeEvent(fd(), events());
}

// 事件多路分发器，如果Channel上有事件发生，则根据
// 不同的事件触发不同的回调
void Channel::handleEvent()
{
    logDebug("fd = %d revents is [%s]", fd(), revstr());
    if (revents() & ERROR)
        if (_errorCb) _errorCb();
    if (revents() & READ)
        if (_readCb) _readCb();
    if (revents() & WRITE)
        if (_writeCb) _writeCb();
}
