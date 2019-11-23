#include <signal.h>

#include "TcpServer.h"
#include "EventLoop.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;
using std::placeholders::_1;

TcpServer::TcpServer(EventLoop *loop, InetAddr listenAddr)
    : _loop(loop),
    _acceptor(new Acceptor(loop, listenAddr)),
    _inetAddr(new InetAddr(listenAddr)),
    _ioThreadPool(new EventLoopThreadPool),
    _threadPool(new ThreadPool),
    _connId(1),
    _connTimeout(-1)
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
    conn->setState(TcpConnection::CONNECTED);
    conn->setConnectionCb(_connectionCb);
    conn->setMessageCb(_messageCb);
    conn->setWriteCompleteCb(_writeCompleteCb);
    conn->setCloseCb([this](const TcpConnectionPtr& conn){
            this->removeConnection(conn);
            });
    _connectionMaps.emplace(id, conn);
    if (_connTimeout > 0) {
        size_t id = ioLoop->runAfter(_connTimeout, [conn]{ conn->close(); });
        conn->setTimeoutTimerId(id);
        conn->setConnTimeout(_connTimeout);
    }
    ioLoop->runInLoop([conn]{ conn->connectEstablish(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    if (_closeCb) _closeCb(conn);
    conn->setState(TcpConnection::CLOSED);
    conn->getLoop()->removeChannel(conn->getChannel());
    _connectionMaps.erase(conn->id());
}

void TcpServer::start()
{
    // 必须忽略SIGPIPE信号，不然当向一个已关闭的连接发送消息时，
    // 会导致服务端意外退出
    addSignal(SIGPIPE, nullptr);
    _acceptor->listen();
    logInfo("server is starting");
}

void TcpServer::quit()
{
    _loop->quit();
}
