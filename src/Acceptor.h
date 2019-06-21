#ifndef _ANGEL_ACCEPTOR_H
#define _ANGEL_ACCEPTOR_H

#include <memory>
#include <functional>
#include "InetAddr.h"
#include "Socket.h"
#include "Buffer.h"
#include "Channel.h"
#include "Noncopyable.h"
#include "decls.h"

namespace Angel {

class EventLoop;

class Acceptor : Noncopyable {
public:
    Acceptor(EventLoop *, InetAddr&);
    ~Acceptor();
    void listen();
    void setNewConnectionCb(const NewConnectionCallback _cb)
    { _newConnectionCb = _cb; }
private:
    void handleAccept();

    EventLoop *_loop;
    std::shared_ptr<Channel> _acceptChannel;
    Socket _socket;
    InetAddr _inetAddr;
    NewConnectionCallback _newConnectionCb;
};
}

#endif // _ANGEL_ACCEPTOR_H
