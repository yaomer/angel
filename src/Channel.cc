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
    "READ | WRITE",
    "ERROR",
};
}

Channel::Channel(EventLoop *loop)
    : _loop(loop)
{

}

Channel::~Channel()
{

}

void Channel::changeEvents()
{
    _loop->changeEvent(fd(), events());
}

void Channel::sendInLoop(const std::string& s)
{
    if (!isWriting() && _output.readable() == 0) {
        ssize_t n = write(fd(), s.data(), s.size());
        LOG_INFO << "write " << n << " bytes: (" << s
                 << ") to fd = " << fd();
        if (n > 0) {
            if (n < s.size()) {
                _output.append(s.data() + n, s.size() - n);
                enableWrite();
            } else {
                if (_writeCompleteCb)
                    _writeCompleteCb();
            }
        } else {
            switch (errno) {
            case EAGAIN:
            // case EWOULDBLOCK:
            case EINTR:
                break;
            default:
                if (_closeCb) _closeCb();
                break;
            }
        }
    } else {
        _output.append(s.data(), s.size());
    }
}

void Channel::send(const std::string& s)
{
    // _loop->runInLoop(std::bind(&Channel::sendInLoop, this, s));
    _loop->runInLoop([s, this]{ sendInLoop(s); });
}

void Channel::handleEvent()
{
    if (revents() & ERROR)
        if (_errorCb) _errorCb();
    if (revents() & READ)
        if (_readCb) _readCb();
    if (revents() & WRITE)
        if (_writeCb) _writeCb();
}

void Channel::handleRead()
{
    ssize_t n = _input.readFd(fd());
    LOG_INFO << "read " << n << " bytes: (" << _input.c_str()
             << ") from fd = " << fd();
    if (n > 0) {
        if (_messageCb)
            _messageCb(shared_from_this(), _input);
    } else if (n == 0) {
        if (_closeCb) _closeCb();
    } else {
        if (_errorCb) _errorCb();
    }
}

void Channel::handleWrite(void)
{
    if (isWriting()) {
        ssize_t n = write(fd(), _output.peek(), _output.readable());
        if (n >= 0) {
            _output.retrieve(n);
            if (_output.readable() == 0) {
                disableWrite();
                if (_writeCompleteCb)
                    _writeCompleteCb();
            }
        } else {
            switch (errno) {
            case EPIPE:
            default:
                if (_closeCb) _closeCb();
                break;
            case EAGAIN:
            // case EWOULDBLOCK: EAGAIN == EWOULDBLOCK
            case EINTR:
                break;
            }
        }
    }
}
