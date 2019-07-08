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
    LOG_INFO << "[TcpConnection::ctor, id = " << _id << "]";
}

TcpConnection::~TcpConnection()
{
    LOG_INFO << "[TcpConnection::dtor, id = " << _id << "]";
}

void TcpConnection::handleRead()
{
    ssize_t n = _input.readFd(_channel->fd());
    LOG_INFO << "read " << n << " bytes: [" << _input.c_str()
             << "] from [fd:" << _channel->fd() << "]";
    if (n > 0) {
        if (_messageCb)
            _messageCb(shared_from_this(), _input);
        else
            _input.retrieveAll();
    } else if (n == 0) {
        handleClose();
    } else {
        handleError();
    }
}

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
                    _loop->runInLoop(
                            [this]{ this->_writeCompleteCb(); });
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
    LOG_INFO << "[fd:" << _channel->fd() << "] is closing";
    if (isSending())
        setWriteCompleteCb([this]{ this->handleClose(); });
    else {
        LOG_INFO << "[fd:" << _channel->fd() << "] is closed";
        _loop->runInLoop(
                [this]{ this->_closeCb(shared_from_this()); });
    }
}

void TcpConnection::handleError()
{
    LOG_ERROR << "fd = " << _channel->fd() << " : " << strerrno();
    handleClose();
}

void TcpConnection::sendInLoop(const std::string& s)
{
    if (!_channel->isWriting() && _output.readable() == 0) {
        ssize_t n = write(_channel->fd(), s.data(), s.size());
        LOG_INFO << "write " << n << " bytes: [" << s << "] to [fd:"
                 << _channel->fd() << "]";
        if (n > 0) {
            if (n < s.size()) {
                _output.append(s.data() + n, s.size() - n);
                _channel->enableWrite();
            } else {
                clearFlag(SENDING);
                if (_writeCompleteCb)
                    _loop->runInLoop(
                            [this]{ this->_writeCompleteCb(); });
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
    } else {
        _output.append(s.data(), s.size());
    }
}

void TcpConnection::send(const std::string& s)
{
    setFlag(SENDING);
    _loop->runInLoop([s, this]{ this->sendInLoop(s); });
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
