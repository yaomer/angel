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

TcpClient::TcpClient(EventLoop *loop,
                     InetAddr& inetAddr,
                     const char *name)
    : _loop(loop),
    _connector(loop, inetAddr),
    _quitLoop(true),
    _name(name)
{
    _connector.setNewConnectionCb(
            std::bind(&TcpClient::newConnection, this, _1));
    logInfo("TcpClient[%s]::ctor", name);
}

TcpClient::~TcpClient()
{
    logInfo("TcpClient[%s]::dtor", name());
}

void TcpClient::newConnection(int fd)
{
    InetAddr localAddr = InetAddr(SockOps::getLocalAddr(fd));
    InetAddr peerAddr = InetAddr(SockOps::getPeerAddr(fd));
    _conn = TcpConnectionPtr(new TcpConnection(1, _loop, fd, localAddr, peerAddr));
    _conn->setState(TcpConnection::CONNECTED);
    _conn->setMessageCb(_messageCb);
    _conn->setCloseCb(
            std::bind(&TcpClient::handleClose, this, _1));
    if (_connectionCb)
        _loop->runInLoop(
                [this]{ this->_connectionCb(this->_conn); });
}

void TcpClient::start()
{
    _connector.connect();
}

void TcpClient::quit()
{
    if (_closeCb) _closeCb(_conn);
    _loop->removeChannel(_conn->getChannel());
    _conn.reset();
    if (_quitLoop) _loop->quit();
}

void TcpClient::notExitFromLoop()
{
    _quitLoop = false;
}
