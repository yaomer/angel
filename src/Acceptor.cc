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
    _listenAddr(listenAddr),
    _idleFd(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    int fd = _socket.fd();
    SockOps::setKeepAlive(fd, true);
    SockOps::setReuseAddr(fd, true);
    SockOps::setNoDelay(fd, true);
    SockOps::setnonblock(fd);
    SockOps::bind(fd, _listenAddr);
    _acceptChannel->setFd(fd);
    logInfo("listenfd = %d", fd);
}

void Acceptor::listen()
{
    SockOps::listen(_socket.fd());
    _acceptChannel->setEventReadCb([this]{ this->handleAccept(); });
    _loop->addChannel(_acceptChannel);
    logInfo("start listening port %d", _listenAddr.toIpPort());
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
    logInfo("accept a new connection[fd=%d]", connfd);
    SockOps::setnonblock(connfd);
    if (_newConnectionCb)
        _newConnectionCb(connfd);
    else
        ::close(connfd);
}
