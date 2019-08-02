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

// Channel用来抽象一个I/O事件，根据注册回调的不同
// 来处理不同的I/O事件。它是基于reactor模式的事件循环中
// 的事件
//
// 对于Channel来说，我们会为它默认开始读事件，因为这是
// 几乎所有类型的事件都需要的，无论是I/O、Timer或Signal，
// 而对于写事件来说，则必须在需要的时候注册，否则会造成
// busy loop，导致CPU空转
class Channel : noncopyable {
public:
    enum EventType {
        READ     = 0x01,
        WRITE    = 0x02,
        ERROR    = 0x04,
    };

    explicit Channel(EventLoop *loop)
        : _loop(loop),
        _fd(-1),
        _events(0),
        _revents(0)
    {
    }
    ~Channel() {  };
    int fd() const { return _fd; }
    int events() const { return _events; }
    const char *evstr();
    int revents() const { return _revents; }
    const char *revstr();
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
    void setEventErrorCb(const EventErrorCallback _cb)
    { _errorCb = std::move(_cb); }
private:
    EventLoop *_loop;
    int _fd;
    int _events;
    int _revents;
    EventReadCallback _readCb;
    EventWriteCallback _writeCb;
    EventErrorCallback _errorCb;
};

extern const char *eventStr[];

}

#endif // _ANGEL_CHANNEL_H
