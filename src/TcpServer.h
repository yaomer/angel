#ifndef _ANGEL_TCPSERVER_H
#define _ANGEL_TCPSERVER_H

#include <map>
#include <set>
#include <functional>
#include <memory>
#include "Acceptor.h"
#include "TcpConnection.h"
#include "InetAddr.h"
#include "EventLoopThreadPool.h"
#include "Id.h"
#include "decls.h"

namespace Angel {

class EventLoop;

class TcpServer {
public:
    explicit TcpServer(EventLoop *, InetAddr&);
    ~TcpServer();
    // Set to TcpConnection::_newConnectionCb
    void newConnection(int fd);
    void removeConnection(const TcpConnectionPtr& conn);
    void setThreadNums(size_t threadNums);
    void start();
    void quit();
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = std::move(_cb); }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = std::move(_cb); }
private:
    size_t getId() { return _connId.getId(); }
    void putId(size_t id) { _connId.putId(id); }
    EventLoop* getNextLoop();

    EventLoop *_loop;
    std::unique_ptr<Acceptor> _acceptor;
    std::unique_ptr<EventLoopThreadPool> _threadPool;
    std::map<size_t, TcpConnectionPtr> _connectionMaps;
    Id _connId;
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
};

}

#endif // _ANGEL_TCPSERVER_H
