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
#include "ThreadPool.h"
#include "Id.h"
#include "decls.h"

namespace Angel {

class EventLoop;

// TcpServer支持以下几种运行方式：
// 1. [单线程reactor]
// 2. [单线程reactor + thread pool](setTaskThreadNums(N > 0))
// 3. [多线程reactor](setIoThreadNums(N > 0))
// 4. [多线程reactor + thread pool](setIoThreadNums(N > 0), setTaskThreadNums(N > 0))
class TcpServer {
public:
    typedef std::map<size_t, TcpConnectionPtr> ConnectionMaps;

    explicit TcpServer(EventLoop *, InetAddr&);
    ~TcpServer();
    void newConnection(int fd);
    void removeConnection(const TcpConnectionPtr& conn);

    ConnectionMaps& connectionMaps() { return _connectionMaps; }
    size_t connectionNums() const { return _connId.getIdNums(); }
    const TcpConnectionPtr& getConnection(size_t id) 
    { return _connectionMaps.find(id)->second; }

    void setIoThreadNums(size_t threadNums)
    { _ioThreadPool->setThreadNums(threadNums); }

    void setTaskThreadNums(size_t threadNums)
    { _threadPool->setThreadNums(threadNums); }

    void executor(const TaskCallback _cb)
    { _threadPool->addTask(std::move(_cb)); }

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
    std::unique_ptr<EventLoopThreadPool> _ioThreadPool;
    ConnectionMaps _connectionMaps;
    std::unique_ptr<ThreadPool> _threadPool;
    Id _connId;
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
};

}

#endif // _ANGEL_TCPSERVER_H
