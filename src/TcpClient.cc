#include "TcpClient.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "SockOps.h"
#include "Connector.h"
#include "TcpConnection.h"
#include "Channel.h"
#include "LogStream.h"

using namespace Angel;
using std::placeholders::_1;

TcpClient::TcpClient(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _connector(loop, inetAddr),
    _flag(0),
    _connectTimeoutTimerId(0)
{
    _connector.setNewConnectionCb(
            std::bind(&TcpClient::newConnection, this, _1));
}

TcpClient::TcpClient(EventLoop *loop,
                     InetAddr& inetAddr,
                     const char *name)
    : _loop(loop),
    _connector(loop, inetAddr),
    _name(name),
    _flag(0),
    _connectTimeoutTimerId(0)
{
    _connector.setNewConnectionCb(
            std::bind(&TcpClient::newConnection, this, _1));
    logInfo("ctor, name = %s", name);
}

TcpClient::~TcpClient()
{
    handleClose(_conn);
    logInfo("dtor, name = %s", name());
}

void TcpClient::newConnection(int fd)
{
    InetAddr localAddr = InetAddr(SockOps::getLocalAddr(fd));
    InetAddr peerAddr = InetAddr(SockOps::getPeerAddr(fd));
    _conn = TcpConnectionPtr(new TcpConnection(1, _loop, fd, localAddr, peerAddr));
    _conn->setState(TcpConnection::CONNECTED);
    _conn->setConnectionCb(_connectionCb);
    _conn->setMessageCb(_messageCb);
    _conn->setCloseCb(
            std::bind(&TcpClient::handleClose, this, _1));
    _loop->runInLoop(
            std::bind(&TcpConnection::connectEstablish, _conn.get()));
}

void TcpClient::start()
{
    _connector.connect();
    if (_connectTimeoutCb)
        _connectTimeoutTimerId = _loop->runAfter(
                _connector.connectWaitTime(), _connectTimeoutCb);
    logInfo("TcpClient[%s] is started", name());
}

void TcpClient::handleClose(const TcpConnectionPtr& conn)
{
    if (_flag & DISCONNECT) return;
    if (_connectTimeoutTimerId > 0)
        _loop->cancelTimer(_connectTimeoutTimerId);
    if (_closeCb) _closeCb(_conn);
    _loop->removeChannel(_conn->getChannel());
    _conn.reset();
    if (!(_flag & NOTEXITLOOP)) _loop->quit();
    _flag |= DISCONNECT;
}

void TcpClient::notExitLoop()
{
    _flag |= NOTEXITLOOP;
}
