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
    LOG_INFO << "TcpClient[" << _name << "]::ctor";
}

TcpClient::~TcpClient()
{
    LOG_INFO << "TcpClient[" << _name << "]::dctor";
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

void TcpClient::handleClose(const TcpConnectionPtr& conn)
{
    if (_closeCb) _closeCb(conn);
    _loop->removeChannel(conn->getChannel());
    _conn.reset();
    LOG_INFO << "TcpClient[" << _name << "] break the connection";
    if (_quitLoop) _loop->quit();
}

void TcpClient::start()
{
    _connector.connect();
}

// 客户端断开连接时是否终止事件循环，默认终止
void TcpClient::quitLoop(bool on)
{
    _quitLoop = on;
}
