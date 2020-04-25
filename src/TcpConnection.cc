#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include "EventLoop.h"
#include "InetAddr.h"
#include "Socket.h"
#include "Channel.h"
#include "TcpServer.h"
#include "TcpConnection.h"
#include "LogStream.h"

using namespace Angel;

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
    _ttl(0),
    _ttlTimerId(0)
{
    _channel->setFd(sockfd);
    _channel->setEventReadCb([this]{ this->handleRead(); });
    _channel->setEventWriteCb([this]{ this->handleWrite(); });
    _channel->setEventErrorCb([this]{ this->handleError(); });
    logInfo("connection(id=%d, fd=%d) is %s", id, sockfd, getStateString());
}

TcpConnection::~TcpConnection()
{
    if (_ttlTimerId > 0)
        _loop->cancelTimer(_ttlTimerId);
    logInfo("connection(id=%d, fd=%d) is %s", _id, _socket->fd(), getStateString());
}

void TcpConnection::connectEstablish()
{
    _loop->addChannel(_channel);
    setState(TcpConnection::CONNECTED);
    logInfo("connection(id=%d, fd=%d) is %s", _id, _socket->fd(), getStateString());
    if (_connectionCb) _connectionCb(shared_from_this());
}

void TcpConnection::handleRead()
{
    ssize_t n = _input.readFd(_channel->fd());
    logDebug("read %zd bytes from connection(id=%d, fd=%d)", n, _id, _channel->fd());
    if (n > 0) {
        if (_messageCb)
            _messageCb(shared_from_this(), _input);
        else {
            // 如果用户未设置消息回调，就丢掉所有读到的数据
            _input.retrieveAll();
        }
    } else if (n == 0) {
        forceCloseConnection();
    } else {
        handleError();
    }
    updateTtlTimerIfNeeded();
}

// 每当关注的sockfd可写时，由handleWrite()负责将未发送完的数据发送过去
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
                    _loop->queueInLoop([conn = shared_from_this()]{
                            conn->_writeCompleteCb(conn);
                            });
                if (_state == CLOSING)
                    forceCloseConnection();
            }
        } else {
            logWarn("write to connection(id=%d, fd=%d): %s", _id, _channel->fd(), strerrno());
            // 对端尚未与我们建立连接或者对端已关闭连接
            if (errno == ECONNRESET || errno == EPIPE) {
                forceCloseConnection();
                return;
            }
        }
    }
}

void TcpConnection::handleClose(bool isForced)
{
    logDebug("connection(id=%d, fd=%d) is %s", _id, _channel->fd(), getStateString());
    if (_state == CLOSED) return;
    if (!isForced && _output.readable() > 0) {
        setState(CLOSING);
        return;
    }
    if (_closeCb)
        _loop->runInLoop([conn = shared_from_this()]{
                conn->_closeCb(conn);
                });
}

void TcpConnection::handleError()
{
    logError("connection(id=%d, fd=%d): %s", _id, _channel->fd(), strerrno());
    // 对端已关闭连接
    if (errno == ECONNRESET)
        forceCloseConnection();
}

void TcpConnection::sendInLoop(const char *data, size_t len)
{
    ssize_t n = 0;
    size_t remainBytes = len;

    if (_state == CLOSED) {
        logWarn("connection(id=%d, fd=%d) is %s, give up sending",
                getStateString(), _id, _channel->fd());
        return;
    }
    if (!_channel->isWriting() && _output.readable() == 0) {
        n = write(_channel->fd(), data, len);
        logDebug("write %zd bytes to connection(id=%d, fd=%d)", n, _id, _channel->fd());
        if (n >= 0) {
            remainBytes = len - n;
            if (remainBytes == 0) {
                if (_writeCompleteCb)
                    _loop->queueInLoop([conn = shared_from_this()]{
                            conn->_writeCompleteCb(conn);
                            });
                if (_state == CLOSING)
                    forceCloseConnection();
            }
        } else {
            logWarn("write to connection(id=%d, fd=%d): %s", _id, _channel->fd(), strerrno());
            if (errno == ECONNRESET || errno == EPIPE) {
                forceCloseConnection();
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

void TcpConnection::send(const char *s, size_t len)
{
    if (_loop->isInLoopThread()) {
        // 在本线程直接发送即可
        sendInLoop(s, len);
    } else {
        // 跨线程必须将数据拷贝一份，防止数据失效
        std::string message(s, len);
        _loop->runInLoop([this, message = std::move(message)]{
                this->sendInLoop(message.data(), message.size());
                });
    }
    updateTtlTimerIfNeeded();
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

void TcpConnection::formatSend(const char *fmt, ...)
{
    va_list ap, ap1;
    va_start(ap, fmt);
    va_copy(ap1, ap);
    int len = vsnprintf(nullptr, 0, fmt, ap1);
    va_end(ap1);
    char buf[len + 1];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    send(buf, len);
}

void TcpConnection::updateTtlTimerIfNeeded()
{
    if (_ttl <= 0) return;
    _loop->cancelTimer(_ttlTimerId);
    _ttlTimerId = _loop->runAfter(_ttl, [conn = shared_from_this()]{
            conn->close();
            });
}

const char *TcpConnection::getStateString()
{
    switch (_state) {
    case CONNECTING: return "CONNECTING";
    case CONNECTED: return "CONNECTED";
    case CLOSING: return "CLOSING";
    case CLOSED: return "CLOSED";
    default: return "NONE";
    }
}
