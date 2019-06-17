#ifndef _ANGEL_CHANNEL_H
#define _ANGEL_CHANNEL_H

#include <memory>
#include <functional>
#include <string>
#include "Buffer.h"
#include "Noncopyable.h"

namespace Angel {

class EventLoop;

class Channel : Noncopyable,
    public std::enable_shared_from_this<Channel> {
public:
    typedef std::function<void()> ReadCallback;
    typedef std::function<void()> WriteCallback;
    typedef std::function<void(std::shared_ptr<Channel>, Buffer&)> MessageCallback;
    typedef std::function<void()> WriteCompleteCallback;
    typedef std::function<void()> CloseCallback;
    typedef std::function<void()> ErrorCallback;

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
    void enableRead() { _events |= READ; }
    void enableWrite() { _events |= WRITE; changeEvents(); }
    void disableWrite() { _events &= ~WRITE; changeEvents(); }
    void changeEvents();
    void sendInLoop(const std::string& s);
    void send(const std::string& s);
    void setReadCb(const ReadCallback _cb)
    { _readCb = _cb; }
    void setWriteCb(const WriteCallback _cb)
    { _writeCb = _cb; }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = _cb; }
    void setWriteCompleteCb(const WriteCompleteCallback _cb)
    { _writeCompleteCb = _cb; }
    void setCloseCb(const CloseCallback _cb)
    { _closeCb = _cb; }
    void setErrorCb(const ErrorCallback _cb)
    { _errorCb = _cb; }
    void close() { if (_closeCb) _closeCb(); }
    void handleEvent();
    void handleRead();
    void handleWrite();
private:
    EventLoop *_loop;
    int _fd = -1;
    int _events = 0;
    int _revents = 0;
    Buffer _input;
    Buffer _output;
    ReadCallback _readCb;
    WriteCallback _writeCb;
    MessageCallback _messageCb;
    WriteCompleteCallback _writeCompleteCb;
    CloseCallback _closeCb;
    ErrorCallback _errorCb;
};

extern const char *eventStr[];

}

#endif // _ANGEL_CHANNEL_H
