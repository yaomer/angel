#ifndef _ANGEL_CONNECTOR
#define _ANGEL_CONNECTOR

#include <memory>
#include <functional>
#include "Socket.h"
#include "InetAddr.h"
#include "Channel.h"
#include "Buffer.h"
#include "Noncopyable.h"

namespace Angel {

class EventLoop;

class Connector : Noncopyable {
public:
    typedef std::function<void(Channel&)> ConnectionCallback;
    typedef std::function<void(std::shared_ptr<Channel>, Buffer&)> MessageCallback;
    Connector(EventLoop *, InetAddr&);
    ~Connector();
    void connect();
    void connecting();
    void connected();
    void timeout();
    void check();
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    Channel& channel() { return *_connect; }
    bool isConnected() { return _connected; }
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = _cb; }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = _cb; }
private:
    EventLoop *_loop;
    Channel *_connect;
    Socket _socket;
    InetAddr _inetAddr;
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
    bool _connected;
};
}

#endif // _ANGEL_CONNECTOR
