#include <unistd.h>
#include <string.h>
#include "EventLoop.h"
#include "InetAddr.h"
#include "Socket.h"
#include "Channel.h"
#include "TcpServer.h"
#include "TcpConnection.h"
#include "LogStream.h"

using namespace Angel;
using std::placeholders::_1;

TcpConnection::TcpConnection(size_t id,
                             EventLoop *loop,
                             int sockfd,
                             InetAddr localAddr,
                             InetAddr peerAddr)
    : _id(id),
    _loop(loop),
    _channel(new Channel(loop)),
    _socket(new Socket(sockfd)),
    _localAddr(localAddr),
    _peerAddr(peerAddr),
    _state(CONNECTING),
    _timeoutTimerId(0),
    _connTimeout(0)
{
    _channel->setFd(sockfd);
    _socket->setReuseAddr(true);
    _channel->setEventReadCb([this]{ this->handleRead(); });
    _channel->setEventWriteCb([this]{ this->handleWrite(); });
    _channel->setEventErrorCb([this]{ this->handleError(); });
    logInfo("[TcpConnection::ctor, id:%d]", _id);
}

TcpConnection::~TcpConnection()
{
    logInfo("[TcpConnection::dtor, id:%d]", _id);
}

void TcpConnection::connectEstablish()
{
    _loop->addChannel(_channel);
    if (_connectionCb) _connectionCb(shared_from_this());
    logInfo("TcpConnection[id = %d] is ESTABLISH", id());
}

void TcpConnection::handleRead()
{
    ssize_t n = _input.readFd(_channel->fd());
    logInfo("read %zd bytes from [fd = %d]", n, _channel->fd());
    if (n > 0) {
        if (_messageCb)
            _messageCb(shared_from_this(), _input);
        else {
            // 如果用户未设置消息回调，就丢掉所有读到的数据
            _input.retrieveAll();
        }
    } else if (n == 0) {
        handleClose();
    } else {
        handleError();
    }
    // 有事件发生后，更新相应的超时定时器
    updateTimeoutTimer();
}

// 每当关注的sockfd可写时，由handleWrite()负责将
// 未发送完的数据发送过去
void TcpConnection::handleWrite()
{
    if (_state == CLOSED)
        return;
    if (_channel->isWriting()) {
        ssize_t n = write(_channel->fd(), _output.peek(),
                _output.readable());
        if (n >= 0) {
            _output.retrieve(n);
            if (_output.readable() == 0) {
                _channel->disableWrite();
                if (_writeCompleteCb)
                    _loop->queueInLoop(
                            std::bind(_writeCompleteCb, shared_from_this()));
                if (_state == CLOSING)
                    handleClose();
            }
        } else {
            logWarn("write: %s", strerrno());
            // 对端尚未与我们建立连接或者对端已关闭连接
            if (errno == ECONNRESET || errno == EPIPE) {
                handleClose();
                return;
            }
        }
    }
}

// 对端关闭连接或连接出错时调用，强制关闭一个连接，
// 未发送完的数据将会被丢弃
void TcpConnection::handleClose()
{
    logInfo("fd = %d is closed", _channel->fd());
    if (_state == CLOSED) return;
    setState(CLOSED);
    if (_closeCb)
        _loop->runInLoop(
                std::bind(_closeCb, shared_from_this()));
}

void TcpConnection::handleError()
{
    logError("[fd = %d]: %s", _channel->fd(), strerrno());
    // 对端已关闭连接
    if (errno == ECONNRESET)
        handleClose();
}

// 服务端主动关闭一个连接时调用，保证停留在缓冲区中的数据
// 发送完后才真正断开连接
void TcpConnection::close()
{
    logInfo("fd = %d is closing", _channel->fd());
    if (_state == CLOSED) return;
    setState(CLOSING);
    if (_closeCb)
        _loop->runInLoop(
                std::bind(_closeCb, shared_from_this()));
}

void TcpConnection::sendInLoop(const char *data, size_t len)
{
    ssize_t n = 0;
    size_t remainBytes = len;

    if (_state == CLOSED) {
        logWarn("TcpConnection[id = %d] is closed, give up sending", id());
        return;
    }
    if (!_channel->isWriting() && _output.readable() == 0) {
        n = write(_channel->fd(), data, len);
        logInfo("write %zd bytes to [fd = %d]", n, _channel->fd());
        if (n >= 0) {
            remainBytes = len - n;
            if (remainBytes == 0) {
                if (_writeCompleteCb)
                    _loop->queueInLoop(
                            std::bind(_writeCompleteCb, shared_from_this()));
                if (_state == CLOSING)
                    close();
            }
        } else {
            logWarn("write: %s", strerrno());
            if (errno == ECONNRESET || errno == EPIPE) {
                handleClose();
                return;
            }
        }
    }
    if (remainBytes > 0) {
        _output.append(data + n, len - n);
        if (!_channel->isWriting())
            _channel->enableWrite();
    }
}

void TcpConnection::sendInNotIoThread(const std::string& data)
{
    sendInLoop(data.data(), data.size());
}

void TcpConnection::send(const char *s, size_t len)
{
    if (_loop->isInLoopThread()) {
        // 在本线程直接发送即可
        sendInLoop(s, len);
    } else {
        // 跨线程必须将数据拷贝一份，防止数据失效
        std::string message(s, len);
        _loop->runInLoop(
                std::bind(&TcpConnection::sendInNotIoThread, this, std::move(message)));
    }
    updateTimeoutTimer();
}

void TcpConnection::send(const char *s)
{
    send(s, strlen(s));
}

void TcpConnection::send(const std::string& s)
{
    send(s.data(), s.size());
}

void TcpConnection::send(const void *v, size_t len)
{
    send(reinterpret_cast<const char*>(v), len);
}

void TcpConnection::updateTimeoutTimer()
{
    if (_timeoutTimerId == 0)
        return;
    _loop->cancelTimer(_timeoutTimerId);
    _timeoutTimerId = _loop->runAfter(
            _connTimeout, [this]{ shared_from_this()->close(); });
}
