#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Channel.h"
#include "SocketOps.h"
#include "LogStream.h"

using namespace Angel;

Acceptor::Acceptor(EventLoop *loop, InetAddr& listenAddr)
    : _loop(loop),
    _acceptChannel(new Channel(loop)),
    _inetAddr(listenAddr)
{
    _socket.socket();
    SocketOps::setnonblock(_socket.fd());
    _socket.setReuseAddr(true);
    _socket.setNoDelay(true);
    _socket.bind(_inetAddr);
    _acceptChannel->setFd(_socket.fd());
    LOG_INFO << "[Acceptor::ctor], listenfd = " << _socket.fd();
}

Acceptor::~Acceptor()
{
    LOG_INFO << "[Acceptor::dtor]";
}

void Acceptor::listen()
{
    _socket.listen();
    _acceptChannel->setEventReadCb([this]{ this->handleAccept(); });
    _loop->addChannel(_acceptChannel);
}

void Acceptor::handleAccept()
{
_again:
    int connfd = _socket.accept();
    if (connfd < 0) {
        switch (errno) {
        case EINTR:
            goto _again;
            break;
        case EWOULDBLOCK: // BSD
        case EPROTO: // SVR4
        case ECONNABORTED: // POSIX
            LOG_WARN << "accept(): " << strerrno();
            break;
        default:
            LOG_ERROR << "accept(): " << strerrno();
            break;
        }
        return;
    }
    if (_newConnectionCb)
        _newConnectionCb(connfd);
    else
        ::close(connfd);
}
