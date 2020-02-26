#include "TcpClient.h"
#include "EventLoop.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;

TcpClient::TcpClient(EventLoop *loop, InetAddr peerAddr)
    : _loop(loop),
    _connector(loop, peerAddr),
    _flag(EXITLOOP)
{
    _connector.setNewConnectionCb([this](int fd){
            this->newConnection(fd);
            });
}

TcpClient::~TcpClient()
{
    handleClose(_conn);
}

void TcpClient::newConnection(int fd)
{
    InetAddr localAddr = InetAddr(SockOps::getLocalAddr(fd));
    InetAddr peerAddr = InetAddr(SockOps::getPeerAddr(fd));
    _conn = TcpConnectionPtr(new TcpConnection(1, _loop, fd, localAddr, peerAddr));
    _conn->setState(TcpConnection::CONNECTED);
    _conn->setConnectionCb(_connectionCb);
    _conn->setMessageCb(_messageCb);
    _conn->setCloseCb([this](const TcpConnectionPtr& conn){
            this->handleClose(conn);
            });
    _loop->runInLoop([conn = this->_conn]{
            conn->connectEstablish();
            });
}

void TcpClient::start()
{
    _connector.connect();
    auto localAddr = InetAddr(SockOps::getLocalAddr(_connector.connfd()));
    logInfo("client [%s:%d] is running", localAddr.toIpAddr(), localAddr.toIpPort());
}

void TcpClient::handleClose(const TcpConnectionPtr& conn)
{
    if (_flag & DISCONNECT) return;
    if (_closeCb) _closeCb(_conn);
    _loop->removeChannel(_conn->getChannel());
    _conn.reset();
    if (_flag & EXITLOOP) _loop->quit();
    _flag |= DISCONNECT;
}
