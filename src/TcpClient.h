#ifndef _ANGEL_TCPCLIENT_H
#define _ANGEL_TCPCLIENT_H

#include <atomic>
#include <string>

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
    TcpClient(EventLoop *, InetAddr&, const char *);
    ~TcpClient();
    void start();
    const char *name() { return _name.c_str(); }
    const TcpConnectionPtr& conn() const { return _conn; }
    void notExitLoop();
    void quit();
    bool isConnected() { return _connector.isConnected(); }
    void setConnectWaitTime(int milliseconds)
    { _connector.setConnectWaitTime(milliseconds); }
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = std::move(_cb); }
    void setConnectTimeoutCb(const ConnectTimeoutCallback _cb)
    { _connectTimeoutCb = std::move(_cb); }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = std::move(_cb); }
    void setCloseCb(const CloseCallback _cb)
    { _closeCb = std::move(_cb); }
private:
    void newConnection(int fd);
    void handleClose(const TcpConnectionPtr&) { quit(); }

    EventLoop *_loop;
    Connector _connector;
    std::shared_ptr<TcpConnection> _conn;
    std::atomic_bool _quitLoop;
    std::string _name;
    ConnectionCallback _connectionCb;
    ConnectTimeoutCallback _connectTimeoutCb;
    MessageCallback _messageCb;
    CloseCallback _closeCb;
};

}

#endif // _ANGEL_TCPCLIENT_H
