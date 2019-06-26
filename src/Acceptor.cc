#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Channel.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;

Acceptor::Acceptor(EventLoop *loop, InetAddr& listenAddr)
    : _loop(loop),
    _acceptChannel(new Channel(loop)),
    _socket(SockOps::socket()),
    _inetAddr(listenAddr)
{
    _socket.setReuseAddr(true);
    _socket.setNoDelay(true);
    SockOps::setnonblock(_socket.fd());
    SockOps::bind(_socket.fd(), &_inetAddr.inetAddr());
    _acceptChannel->setFd(_socket.fd());
    LOG_INFO << "[Acceptor::ctor], listenfd = " << _socket.fd();
}

Acceptor::~Acceptor()
{
    LOG_INFO << "[Acceptor::dtor]";
}

void Acceptor::listen()
{
    SockOps::listen(_socket.fd());
    _acceptChannel->setEventReadCb([this]{ this->handleAccept(); });
    _loop->addChannel(_acceptChannel);
}

void Acceptor::handleAccept()
{
_again:
    int connfd = SockOps::accept(_socket.fd());
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
