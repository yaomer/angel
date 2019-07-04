#ifndef _ANGEL_CHANNEL_H
#define _ANGEL_CHANNEL_H

#include <functional>
#include <string>
#include <atomic>
#include "Buffer.h"
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class EventLoop;

class Channel : noncopyable {
public:
    enum EventType {
        READ     = 0x01,
        WRITE    = 0x02,
        ERROR    = 0x04,
    };

    explicit Channel(EventLoop *loop);
    ~Channel();
    int fd() const { return _fd; }
    int events() const { return _events; }
    int revents() const { return _revents; }
    void setFd(int fd) { _fd = fd; }
    void setRevents(int revents) { _revents = revents; }
    bool isReading() { return _events & READ; }
    bool isWriting() { return _events & WRITE; }
    void enableRead();
    void enableWrite();
    void disableWrite();
    void changeEvents();
    void handleEvent();
    void setEventReadCb(const EventReadCallback _cb)
    { _readCb = std::move(_cb); }
    void setEventWriteCb(const EventWriteCallback _cb)
    { _writeCb = std::move(_cb); }
    void setEventCloseCb(const EventCloseCallback _cb)
    { _closeCb = std::move(_cb); }
    void setEventErrorCb(const EventErrorCallback _cb)
    { _errorCb = std::move(_cb); }
private:
    EventLoop *_loop;
    int _fd;
    int _events;
    int _revents;
    EventReadCallback _readCb;
    EventWriteCallback _writeCb;
    EventCloseCallback _closeCb;
    EventErrorCallback _errorCb;
};

extern const char *eventStr[];

}

#endif // _ANGEL_CHANNEL_H
