#include <unistd.h>
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
    logInfo("[Acceptor::ctor, listenfd = %d]", _socket.fd());
}

Acceptor::~Acceptor()
{
    logInfo("[Acceptor::dtor]");
}

void Acceptor::listen()
{
    SockOps::listen(_socket.fd());
    _acceptChannel->setEventReadCb([this]{ this->handleAccept(); });
    _loop->addChannel(_acceptChannel);
}

void Acceptor::handleAccept()
{
    int connfd = SockOps::accept(_socket.fd());
    if (connfd < 0) {
        switch (errno) {
        case EINTR:
        case EWOULDBLOCK: // BSD
        case EPROTO: // SVR4
        case ECONNABORTED: // POSIX
            break;
        default:
            logError("accept: %s", strerrno());
            break;
        }
        return;
    }
    logInfo("[fd = %d is connected]", connfd);
    if (_newConnectionCb)
        _newConnectionCb(connfd);
    else
        ::close(connfd);
}
