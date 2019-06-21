#include "TcpClient.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "SocketOps.h"
#include "Connector.h"
#include "TcpConnection.h"
#include "Channel.h"
#include "LogStream.h"

using namespace Angel;
using std::placeholders::_1;

TcpClient::TcpClient(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _connector(loop, inetAddr)
{
    _connector.setNewConnectionCb(
            std::bind(&TcpClient::newConnection, this, _1));
    LOG_INFO << "[TcpClient::ctor]";
}

TcpClient::~TcpClient()
{
    LOG_INFO << "[TcpClient::dtor]";
}

void TcpClient::newConnection(int fd)
{
    InetAddr localAddr = InetAddr(SocketOps::getLocalAddr(fd));
    InetAddr peerAddr = InetAddr(SocketOps::getPeerAddr(fd));
    _connection = TcpConnectionPtr(new TcpConnection(1, _loop, fd,
                localAddr, peerAddr));
    _connection->setMessageCb(_messageCb);
    if (_connectionCb) _connectionCb(_connection);
    // _connection->setConnectionCb(_connectionCb);
    _connection->setCloseCb(
            std::bind(&TcpClient::handleClose, this, _1));
    _connection->setState(TcpConnection::CONNECTED);
}

void TcpClient::handleClose(const TcpConnectionPtr& conn)
{
    quit();
}

void TcpClient::start()
{
    _connector.connect();
}

void TcpClient::quit()
{
    _loop->quit();
}
