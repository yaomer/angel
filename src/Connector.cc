#include <errno.h>
#include <unistd.h>

#include "EventLoop.h"
#include "Socket.h"
#include "SockOps.h"
#include "Connector.h"
#include "LogStream.h"

using namespace Angel;

void Connector::connect()
{
    _waitRetry = false;
    logInfo("connect -> [%s:%d]", _peerAddr.toIpAddr(), _peerAddr.toIpPort());
    int sockfd = SockOps::socket();
    SockOps::setnonblock(sockfd);
    int ret = SockOps::connect(sockfd, _peerAddr);
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
    logDebug("fd=%d is connecting", sockfd);
    _connectChannel->setEventReadCb(
            [this, sockfd]{ this->check(sockfd); });
    _connectChannel->setEventWriteCb(
            [this, sockfd]{ this->check(sockfd); });
    _connectChannel->enableWrite();
}

void Connector::connected(int sockfd)
{
    if (_connected) return;
    logDebug("fd=%d is connected", sockfd);
    _loop->removeChannel(_connectChannel);
    _connectChannel.reset();
    if (_newConnectionCb) {
        _newConnectionCb(sockfd);
        _connected = true;
    }
}

// 在Mac OS上使用poll判断sockfd的状态，好像有时会可读，有时会可写
// 所以我们在可读可写情况下都使用getsockopt来获取sockfd的状态

// 如果一个socket fd上出现了错误，那么在close(fd)之前，只能通过
// getsockopt()获取一次错误，之后便不会再触发

void Connector::check(int sockfd)
{
    if (_waitRetry) return;
    int err = SockOps::getSocketError(sockfd);
    if (err) {
        logError("connect: %s, try to reconnect after %d s", strerr(err), retry_interval);
        retry(sockfd);
        return;
    }
    connected(sockfd);
}

void Connector::retry(int sockfd)
{
    _loop->removeChannel(_connectChannel);
    _loop->runAfter(1000 * retry_interval, [this]{ this->connect(); });
    _waitRetry = true;
    ::close(sockfd);
}
