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
    _threadPool(new EventLoopThreadPool),
    _connId(1)
{
    _acceptor->setNewConnectionCb(
            std::bind(&TcpServer::newConnection, this, _1));
    LOG_INFO << "[TcpServer::ctor]";
}

TcpServer::~TcpServer()
{
    LOG_INFO << "[TcpServer::dtor]";
}

void TcpServer::setThreadNums(size_t threadNums)
{
    _threadPool->setThreadNums(threadNums);
}

EventLoop *TcpServer::getNextLoop()
{
    if (_threadPool->threadNums() > 0)
        return _threadPool->getNextThread()->getAssertTrueLoop();
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
    conn->setCloseCb(
            std::bind(&TcpServer::removeConnection, this, _1));
    if (_connectionCb)
        ioLoop->runInLoop([this, conn]{ this->_connectionCb(conn); });
    _connectionMaps[id] = std::move(conn);
    LOG_INFO << "[fd:" << fd << "] is connected";
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    putId(conn->id());
    conn->getLoop()->removeChannel(conn->getChannel());
    _connectionMaps.erase(conn->id());
}

void TcpServer::start()
{
    _acceptor->listen();
}

void TcpServer::quit()
{
    _loop->quit();
    _loop->wakeup();
}
