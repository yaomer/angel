#include "TcpClient.h"
#include "EventLoop.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;
using std::placeholders::_1;

TcpClient::TcpClient(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _connector(loop, inetAddr),
    _flag(0)
{
    _connector.setNewConnectionCb(
            std::bind(&TcpClient::newConnection, this, _1));
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
    _conn->setCloseCb(
            std::bind(&TcpClient::handleClose, this, _1));
    _loop->runInLoop(
            std::bind(&TcpConnection::connectEstablish, _conn.get()));
}

void TcpClient::start()
{
    _connector.connect();
    logInfo("client is starting");
}

void TcpClient::handleClose(const TcpConnectionPtr& conn)
{
    if (_flag & DISCONNECT) return;
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
