#ifndef _ANGEL_TCPCLIENT_H
#define _ANGEL_TCPCLIENT_H

#include <string>

#include "Connector.h"
#include "TcpConnection.h"
#include "InetAddr.h"
#include "Channel.h"
#include "Buffer.h"
#include "decls.h"
#include "noncopyable.h"

namespace Angel {

class EventLoop;

class TcpClient : noncopyable {
public:
    enum Flag { NOTEXITLOOP = 01, DISCONNECT = 02 };
    TcpClient(EventLoop *, InetAddr);
    ~TcpClient();
    void start();
    const char *name() { return _name.c_str(); }
    void setName(const std::string& name) { _name = name; }
    const TcpConnectionPtr& conn() const { return _conn; }
    void notExitLoop();
    bool isConnected() { return _connector.isConnected() && _conn; }
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = std::move(_cb); }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = std::move(_cb); }
    void setCloseCb(const CloseCallback _cb)
    { _closeCb = std::move(_cb); }
private:
    void newConnection(int fd);
    void handleClose(const TcpConnectionPtr&);

    EventLoop *_loop;
    Connector _connector;
    std::shared_ptr<TcpConnection> _conn;
    std::string _name;
    int _flag;
    // 连接刚建立后被调用
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
    // 对端关闭连接时被调用
    CloseCallback _closeCb;
};

}

#endif // _ANGEL_TCPCLIENT_H
