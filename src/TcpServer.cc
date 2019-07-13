#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;
using std::placeholders::_1;

TcpServer::TcpServer(EventLoop *loop, InetAddr& listenAddr)
    : _loop(loop),
    _acceptor(new Acceptor(loop, listenAddr)),
    _ioThreadPool(new EventLoopThreadPool),
    _threadPool(new ThreadPool),
    _connId(1)
{
    _acceptor->setNewConnectionCb(
            std::bind(&TcpServer::newConnection, this, _1));
    logInfo("[TcpServer::ctor]");
}

TcpServer::~TcpServer()
{
    logInfo("[TcpServer::dtor]");
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
    size_t id = getId();
    EventLoop *ioLoop = getNextLoop();
    InetAddr localAddr = SockOps::getLocalAddr(fd);
    InetAddr peerAddr = SockOps::getPeerAddr(fd);
    TcpConnectionPtr conn(new TcpConnection(id, ioLoop, fd, localAddr, peerAddr));
    conn->setState(TcpConnection::CONNECTED);
    conn->setMessageCb(_messageCb);
    conn->setWriteCompleteCb(_writeCompleteCb);
    conn->setCloseCb(
            std::bind(&TcpServer::removeConnection, this, _1));
    _connectionMaps[id] = conn;
    if (_connectionCb)
        ioLoop->runInLoop(
                [this, conn]{ this->_connectionCb(conn); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    if (_closeCb) _closeCb(conn);
    logInfo("[fd:%d] is closed", conn->getChannel()->fd());
    conn->setState(TcpConnection::CLOSED);
    putId(conn->id());
    conn->getLoop()->removeChannel(conn->getChannel());
    _connectionMaps.erase(conn->id());
}

void TcpServer::start()
{
    // 必须忽略SIGPIPE信号，不然当向一个已关闭的连接发送消息时，
    // 会导致服务端意外退出
    addSignal(SIGPIPE, nullptr);
    _acceptor->listen();
}

void TcpServer::quit()
{
    _loop->quit();
}
