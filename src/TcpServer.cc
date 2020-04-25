#include <signal.h>

#include "TcpServer.h"
#include "EventLoop.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;

TcpServer::TcpServer(EventLoop *loop, InetAddr listenAddr)
    : _loop(loop),
    _acceptor(new Acceptor(loop, listenAddr)),
    _listenAddr(new InetAddr(listenAddr)),
    _ioThreadPool(new EventLoopThreadPool),
    _threadPool(new ThreadPool),
    _connId(1), // 预留下0，可能会用到
    _ttl(0)    // 默认禁用TTL
{
    _acceptor->setNewConnectionCb([this](int fd){
            this->newConnection(fd);
            });
}

EventLoop *TcpServer::getNextLoop()
{
    if (_ioThreadPool->threadNums() > 0)
        return _ioThreadPool->getNextThread()->getAssertTrueLoop();
    else
        return _loop;
}

void TcpServer::newConnection(int fd)
{
    size_t id = _connId++;
    EventLoop *ioLoop = getNextLoop();
    InetAddr localAddr = SockOps::getLocalAddr(fd);
    InetAddr peerAddr = SockOps::getPeerAddr(fd);
    TcpConnectionPtr conn(new TcpConnection(id, ioLoop, fd, localAddr, peerAddr));
    conn->setConnectionCb(_connectionCb);
    conn->setMessageCb(_messageCb);
    conn->setWriteCompleteCb(_writeCompleteCb);
    conn->setCloseCb([this](const TcpConnectionPtr& conn){
            this->removeConnection(conn);
            });
    _connectionMaps.emplace(id, conn);
    setTtlTimerIfNeeded(ioLoop, conn);
    ioLoop->runInLoop([conn]{ conn->connectEstablish(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    // 这里禁用conn的TTL，因为如果用户开启了TTL，并且在_closeCb()中发送了数据，
    // 而conn->send()中会更新连接的ttl timer，从而会导致conn的生命期被延长，
    // 即再经过ttl时间后才会被关闭
    conn->setTtl(0, 0);
    if (_closeCb) _closeCb(conn);
    conn->setState(TcpConnection::CLOSED);
    conn->getLoop()->removeChannel(conn->getChannel());
    // 必须在主线程的EventLoop中移除一个连接，否则多个线程就有可能并发
    // 修改connectionMaps
    // 例如，主线程接收到一个新连接，之后调用newConnection将它添加到
    // connectionMaps中，如果此时恰好有某个io子线程要移除一个连接，并且
    // 正在调用removeConnection，这时两个线程就会同时修改connectionMaps，
    // 这会导致难以预料的后果
    _loop->runInLoop([this, id = conn->id()]{
            this->connectionMaps().erase(id);
            });
}

void TcpServer::setTtlTimerIfNeeded(EventLoop *loop, const TcpConnectionPtr& conn)
{
    if (_ttl <= 0) return;
    size_t id = loop->runAfter(_ttl, [conn]{ conn->close(); });
    conn->setTtl(id, _ttl);
}

void TcpServer::start()
{
    logInfo("server [%s:%d] is running", _listenAddr->toIpAddr(), _listenAddr->toIpPort());
    // 必须忽略SIGPIPE信号，不然当向一个已关闭的连接发送消息时，会导致服务端意外退出
    addSignal(SIGPIPE, nullptr);
    _acceptor->listen();
}

void TcpServer::quit()
{
    _loop->quit();
}
