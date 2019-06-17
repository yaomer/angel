#ifndef _ANGEL_ACCEPTOR_H
#define _ANGEL_ACCEPTOR_H

#include <memory>
#include <functional>
#include "InetAddr.h"
#include "Socket.h"
#include "Buffer.h"
#include "Noncopyable.h"

namespace Angel {

class EventLoop;
class Channel;

class Acceptor : Noncopyable {
public:
    typedef std::function<void(Channel&)> AcceptionCallback;
    typedef std::function<void(std::shared_ptr<Channel>, Buffer&)> MessageCallback;
    Acceptor(EventLoop *, InetAddr&);
    ~Acceptor();
    void handleAccept();
    void handleClose(Channel *);
    void registerNewChannel(int fd);
    void listen();
    void setAcceptionCb(const AcceptionCallback _cb)
    { _acceptionCb = _cb; }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = _cb; }
private:
    EventLoop *_loop;
    Channel *_accept;
    InetAddr _inetAddr;
    Socket _socket;
    AcceptionCallback _acceptionCb;
    MessageCallback _messageCb;
};
}

#endif // _ANGEL_ACCEPTOR_H
