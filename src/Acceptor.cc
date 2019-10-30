#include <unistd.h>
#include <fcntl.h>

#include "Acceptor.h"
#include "EventLoop.h"
#include "SockOps.h"
#include "LogStream.h"

using namespace Angel;

Acceptor::Acceptor(EventLoop *loop, InetAddr& listenAddr)
    : _loop(loop),
    _acceptChannel(new Channel(loop)),
    _socket(SockOps::socket()),
    _inetAddr(listenAddr),
    _idleFd(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    _socket.setKeepAlive(true);
    _socket.setReuseAddr(true);
    _socket.setNoDelay(true);
    SockOps::setnonblock(_socket.fd());
    SockOps::bind(_socket.fd(), &_inetAddr.inetAddr());
    _acceptChannel->setFd(_socket.fd());
    logInfo("listenfd = %d, listenAddr = [%s:%d]", _socket.fd(),
            _inetAddr.toIpAddr(), _inetAddr.toIpPort());
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
        case EMFILE:
            ::close(_idleFd);
            connfd = SockOps::accept(_socket.fd());
            ::close(connfd);
            _idleFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            break;
        default:
            logError("accept: %s", strerrno());
            break;
        }
        return;
    }
    SockOps::setnonblock(connfd);
    logInfo("fd = %d is connected", connfd);
    if (_newConnectionCb)
        _newConnectionCb(connfd);
    else
        ::close(connfd);
}
