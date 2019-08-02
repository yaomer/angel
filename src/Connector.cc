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
    logInfo("[connect -> %s:%d]", _peerAddr.toIpAddr(),
            _peerAddr.toIpPort());
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
    logInfo("[connfd = %d] is connecting", sockfd);
    _loop->runAfter(1000 * _waitTime, [this]{ this->timeout(); });
    _connectChannel->setEventReadCb(
            [this, sockfd]{ this->check(sockfd); });
    _connectChannel->setEventWriteCb(
            [this, sockfd]{ this->check(sockfd); });
    _connectChannel->enableWrite();
}

void Connector::connected(int sockfd)
{
    if (_connected) return;
    logInfo("[connfd = %d] is connected", sockfd);
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
            logFatal("connect timeout: %d s", _waitAllTime);
        logInfo("connect: waited %d s", _waitTime);
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
        logFatal("connect: %s", strerr(err));
    }
    connected(sockfd);
}
