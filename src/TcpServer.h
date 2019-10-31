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
#include "decls.h"
#include "noncopyable.h"

namespace Angel {

class EventLoop;

// TcpServer支持以下几种运行方式：
// 1. [单线程reactor]
// 2. [单线程reactor + thread pool](setTaskThreadNums(N > 0))
// 3. [多线程reactor](setIoThreadNums(N > 0))
// 4. [多线程reactor + thread pool](setIoThreadNums(N > 0), setTaskThreadNums(N > 0))
class TcpServer : noncopyable {
public:
    typedef std::map<size_t, TcpConnectionPtr> ConnectionMaps;

    explicit TcpServer(EventLoop *, InetAddr);
    void newConnection(int fd);
    void removeConnection(const TcpConnectionPtr& conn);
    InetAddr *inetAddr() { return _inetAddr.get(); }

    ConnectionMaps& connectionMaps() { return _connectionMaps; }
    size_t clients() const { return _connectionMaps.size(); };
    TcpConnection *getConnection(size_t id)
    {
        auto it = _connectionMaps.find(id);
        if (it != _connectionMaps.cend())
            return it->second.get();
        else
            return nullptr;
    }

    void setIoThreadNums(size_t threadNums)
    { _ioThreadPool->start(threadNums); }

    void setTaskThreadNums(size_t threadNums)
    { _threadPool->start(threadNums); }

    void executor(const TaskCallback _cb)
    { _threadPool->addTask(std::move(_cb)); }

    void setConnectionTimeout(int64_t timeout)
    { _connTimeout = timeout; }

    void start();
    void quit();
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = std::move(_cb); }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = std::move(_cb); }
    void setCloseCb(const CloseCallback _cb)
    { _closeCb = std::move(_cb); }
    void setWriteCompleteCb(const WriteCompleteCallback _cb)
    { _writeCompleteCb = std::move(_cb); }
private:
    EventLoop* getNextLoop();

    EventLoop *_loop;
    std::unique_ptr<Acceptor> _acceptor;
    std::unique_ptr<InetAddr> _inetAddr;
    std::unique_ptr<EventLoopThreadPool> _ioThreadPool;
    // 只有主线程在接收到一个新连接时会将其添加到ConnectionMaps中，
    // 而在除此之外的其他地方则不会修改ConnectionMaps
    //
    // 当服务器以多线程方式运行时，就有可能会出现并发访问ConnectionMaps的问题
    // 解决方法通常有两种：
    // 1）由TcpServer提供一把锁，在访问ConnectionMaps时必须先加锁
    // 2）业务逻辑将涉及到ConnectionMaps的操作放入主线程中执行(通过queueInLoop())
    ConnectionMaps _connectionMaps;
    std::unique_ptr<ThreadPool> _threadPool;
    size_t _connId;
    int64_t _connTimeout;
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
    CloseCallback _closeCb;
    WriteCompleteCallback _writeCompleteCb;
};

}

#endif // _ANGEL_TCPSERVER_H
