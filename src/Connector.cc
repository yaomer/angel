#include <errno.h>
#include "EventLoop.h"
#include "Channel.h"
#include "Socket.h"
#include "Connector.h"
#include "LogStream.h"

using namespace Angel;

Connector::Connector(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _connect(new Channel(loop)),
    _inetAddr(inetAddr),
    _connected(false),
    _waitTime(2)
{
    _socket.socket();
    _socket.setNonBlock();
    _connect->setFd(_socket.fd());
    _loop->addChannel(_connect);
}

Connector::~Connector()
{

}

void Connector::connect()
{
    int ret = _socket.connect(_inetAddr);
    if (ret == 0) {
        // 通常如果服务端和客户端在同一台主机，连接会立即建立
        connected();
    } else if (ret < 0) {
        if (errno == EINPROGRESS) {
            // 连接正在建立
            connecting();
        }
    }
}

void Connector::connecting()
{
    // 2 4 8 16
    LOG_INFO << "connfd = " << _socket.fd() << " is connecting";
    _loop->runAfter(1000 * _waitTime, [this]{ this->timeout(); });
    _connect->setReadCb([this]{ this->check(); });
    _connect->setWriteCb([this]{ this->check(); });
    _connect->setErrorCb([this]{ this->handleError(); });
    _connect->enableWrite();
}

void Connector::connected()
{
    _connected = true;
    LOG_INFO << "connfd = " << _socket.fd() << " is connected";
    _connect->setReadCb([this]{ this->_connect->handleRead(); });
    _connect->setWriteCb([this]{ this->_connect->handleWrite(); });
    _connect->setMessageCb(_messageCb);
    _connect->setCloseCb([this]{ this->handleClose(); });
    _connect->disableWrite();
}

void Connector::timeout()
{
    if (!isConnected()) {
        if (_waitTime == _waitMaxTime)
            LOG_FATAL << "connect timeout: " << _waitAllTime << " s";
        LOG_INFO << "connect: waited " << _waitTime << " s";
        _waitTime *= 2;
        _loop->runAfter(1000 * _waitTime, [this]{ this->timeout(); });
    }
}

// 在Mac OS上使用poll判断sockfd的状态，好像有时会可读，有时会可写
// 所以我们在可读可写情况下都使用getsockopt来获取sockfd的状态

void Connector::check()
{
    int err = _socket.socketError();
    if (err) {
        LOG_FATAL << "connect: " << strerr(err);
    }
    connected();
    if (_connectionCb) _connectionCb(*_connect);
}

void Connector::handleClose()
{
    LOG_INFO << "server closed connection";
    _loop->quit();
}

void Connector::handleError()
{
    LOG_ERROR << strerrno();
}
