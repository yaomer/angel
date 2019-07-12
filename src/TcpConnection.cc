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
    _flag(0)
{
    _channel->setFd(sockfd);
    _channel->setEventReadCb([this]{ this->handleRead(); });
    _channel->setEventWriteCb([this]{ this->handleWrite(); });
    _channel->setEventCloseCb([this]{ this->handleClose(); });
    _channel->setEventErrorCb([this]{ this->handleError(); });
    _loop->addChannel(_channel);
    logInfo("[TcpConnection::ctor, id:%d]", _id);
}

TcpConnection::~TcpConnection()
{
    logInfo("[TcpConnection::dtor, id:%d]", _id);
}

void TcpConnection::handleRead()
{
    ssize_t n = _input.readFd(_channel->fd());
    logInfo("read %zd bytes: [%s] from [fd:%d]", n, _input.c_str(), _channel->fd());
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
}

// 每当关注的sockfd可写时，由handleWrite()负责将
// 未发送完的数据发送过去
void TcpConnection::handleWrite()
{
    if (_channel->isWriting()) {
        ssize_t n = write(_channel->fd(), _output.peek(),
                _output.readable());
        if (n >= 0) {
            _output.retrieve(n);
            if (_output.readable() == 0) {
                _channel->disableWrite();
                clearFlag(SENDING);
                if (_writeCompleteCb)
                    _loop->queueInLoop(
                            std::bind(_writeCompleteCb, shared_from_this()));
                if (_state == CLOSING)
                    _closeCb(shared_from_this());
            }
        } else {
            switch (errno) {
            case EPIPE:
            default:
                handleClose();
                break;
            case EAGAIN:
            // case EWOULDBLOCK: EAGAIN == EWOULDBLOCK
            case EINTR:
                break;
            }
        }
    }
}

void TcpConnection::handleClose()
{
    logInfo("[fd:%d] is closing", _channel->fd());
    if (isSending()) {
        // 如果正在发送数据，就不能直接关闭连接，需要等数据发送完后再关闭
        setState(CLOSING);
    } else {
        logInfo("[fd:%d] is closed", _channel->fd());
        _loop->runInLoop(
                std::bind(_closeCb, shared_from_this()));
    }
}

void TcpConnection::handleError()
{
    logError("[fd:%d]: %s", _channel->fd(), strerrno());
}

void TcpConnection::sendInLoop(const std::string& s)
{
    ssize_t n = 0;
    size_t remainBytes = s.size();

    if (_state == CLOSED)
        return;
    if (!_channel->isWriting() && _output.readable() == 0) {
        n = write(_channel->fd(), s.data(), s.size());
        logInfo("write %zd bytes: [%s] to [fd:%d]", n, s.c_str(), _channel->fd());
        if (n >= 0) {
            remainBytes = s.size() - n;
            if (remainBytes == 0) {
                clearFlag(SENDING);
                if (_writeCompleteCb)
                    _loop->queueInLoop(
                            std::bind(_writeCompleteCb, shared_from_this()));
                if (_state == CLOSING)
                    _closeCb(shared_from_this());
            }
        } else {
            switch (errno) {
            case EAGAIN:
            // case EWOULDBLOCK:
            case EINTR:
                break;
            default:
                handleClose();
                break;
            }
        }
    }
    if (remainBytes > 0) {
        _output.append(s.data() + n, s.size() - n);
        if (!_channel->isWriting())
            _channel->enableWrite();
    }
}

void TcpConnection::send(const std::string& s)
{
    setFlag(SENDING);
    if (_loop->isInLoopThread()) {
        // 在本线程直接发送即可
        sendInLoop(s);
    } else {
        // 跨线程必须将数据拷贝一份，防止数据失效
        _loop->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, std::string(s)));
    }
}

void TcpConnection::send(const char *s)
{
    send(std::move(std::string(s)));
}

void TcpConnection::send(const char *s, size_t len)
{
    send(std::move(std::string(s, len)));
}

void TcpConnection::send(const void *v, size_t len)
{
    std::string s(len, 0);
    const char *vptr = reinterpret_cast<const char*>(v);
    std::copy(vptr, vptr + len, s.begin());
    send(std::move(s));
}
