#include <errno.h>
#include "EventLoop.h"
#include "Channel.h"
#include "Socket.h"
#include "SockOps.h"
#include "InetAddr.h"
#include "Connector.h"
#include "LogStream.h"

using namespace Angel;

Connector::Connector(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _connectChannel(new Channel(loop)),
    _peerAddr(inetAddr),
    _connected(false),
    _waitTime(2)
{
    LOG_INFO << "[Connector::ctor] -> [" << _peerAddr.toIpAddr() << ":"
             << _peerAddr.toIpPort() << "]";
}

Connector::~Connector()
{
    LOG_INFO << "[Connector::dtor]";
}

void Connector::connect()
{
    int sockfd = SockOps::socket();
    SockOps::setnonblock(sockfd);
    int ret = SockOps::connect(sockfd, &_peerAddr.inetAddr());
    _connectChannel->setFd(sockfd);
    _loop->addChannel(_connectChannel);
    if (ret == 0) {
        // 通常如果服务端和客户端在同一台主机，连接会立即建立
        connected(sockfd);
    } else if (ret < 0) {
        if (errno == EINPROGRESS) {
            // 连接正在建立
            connecting(sockfd);
        }
    }
}

void Connector::connecting(int sockfd)
{
    LOG_INFO << "[connfd:" << sockfd << "] is connecting";
    _loop->runAfter(1000 * _waitTime, [this]{ this->timeout(); });
    _connectChannel->setEventReadCb(
            [this, sockfd]{ this->check(sockfd); });
    _connectChannel->setEventWriteCb(
            [this, sockfd]{ this->check(sockfd); });
    _connectChannel->enableWrite();
}

void Connector::connected(int sockfd)
{
    LOG_INFO << "[connfd:" << sockfd << "] is connected";
    _loop->removeChannel(_connectChannel);
    _connectChannel.reset();
    if (_newConnectionCb) {
        _newConnectionCb(sockfd);
        _connected = true;
    } else
        _loop->quit();
}

void Connector::timeout()
{
    if (!_connected) {
        if (_waitTime == _waitMaxTime)
            LOG_FATAL << "connect timeout: " << _waitAllTime << " s";
        LOG_INFO << "connect: waited " << _waitTime << " s";
        _waitTime *= 2;
        _loop->runAfter(1000 * _waitTime, [this]{ this->timeout(); });
    }
}

// 在Mac OS上使用poll判断sockfd的状态，好像有时会可读，有时会可写
// 所以我们在可读可写情况下都使用getsockopt来获取sockfd的状态

void Connector::check(int sockfd)
{
    int err = SockOps::getSocketError(sockfd);
    if (err) {
        LOG_FATAL << "connect: " << strerr(err);
    }
    connected(sockfd);
}
