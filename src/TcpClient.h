#ifndef _ANGEL_TCPCLIENT_H
#define _ANGEL_TCPCLIENT_H

#include "Connector.h"
#include "TcpConnection.h"
#include "InetAddr.h"
#include "Channel.h"
#include "Buffer.h"
#include "decls.h"

namespace Angel {

class EventLoop;

class TcpClient {
public:
    TcpClient(EventLoop *, InetAddr&);
    ~TcpClient();
    void start();
    void quit();
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = _cb; }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = _cb; }
private:
    void newConnection(int fd);
    void handleClose(const TcpConnectionPtr&);

    EventLoop *_loop;
    Connector _connector;
    std::shared_ptr<TcpConnection> _connection;
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
};

}

#endif // _ANGEL_TCPCLIENT_H
