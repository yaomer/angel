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

// connect()之后连接不会马上建立，如果此时立刻需要使用client.conn()，
// 则必须保证它为真，exp:
// ...
// client.start();
// client.conn()->send("hello\n");
// 上面代码中client.conn()很可能为假，所以可以
// while (!client.conn()) ;
// 稍微等待一会
void TcpClient::start()
{
    _connector.connect();
    if (_connectTimeoutCb)
        _loop->runAfter(_connector.connectWaitTime(), _connectTimeoutCb);
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

void TcpClient::retryWithPerSec()
{
    // TODO: connect超时后，每隔1秒尝试一次重连
}

void TcpClient::retryWithExpBackoff()
{
    // TODO: connect超时后，以指数退避方式进行重连
}
