#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Channel.h"
#include "LogStream.h"

using namespace Angel;

Acceptor::Acceptor(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _accept(new Channel(loop)),
    _inetAddr(inetAddr)
{
    _socket.socket();
    LOG_DEBUG << "listenfd = " << _socket.fd();
    _socket.setNonBlock();
    _socket.setReuseAddr(true);
    _socket.setNoDelay(true);
    _socket.bind(_inetAddr);
    _accept->setFd(_socket.fd());
}

Acceptor::~Acceptor()
{

}

void Acceptor::listen()
{
    _socket.listen();
    _accept->setReadCb([this]{ this->handleAccept(); });
    _loop->addChannel(_accept);
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
            LOG_WARN << "accept warnning: " << strerrno();
            break;
        default:
            LOG_ERROR << "accept error: " << strerrno();
            break;
        }
    }
    _socket.setNonBlock(connfd);
    registerNewChannel(connfd);
}

void Acceptor::registerNewChannel(int fd)
{
    LOG_INFO << "fd = " << fd << " is connected";
    Channel *chl = new Channel(_loop);
    chl->setFd(fd);
    chl->setReadCb([chl]{ chl->handleRead(); });
    chl->setWriteCb([chl]{ chl->handleWrite(); });
    chl->setMessageCb(_messageCb);
    chl->setCloseCb([this, chl]{ this->handleClose(chl); });
    _loop->addChannel(chl);
    if (_acceptionCb) _acceptionCb(*chl);
}

void Acceptor::handleClose(Channel *chl)
{
    LOG_INFO << "fd = " << chl->fd() << " is closing";
    if (chl->isWriting())
        chl->setWriteCompleteCb([this, chl]{ this->handleClose(chl); });
    else {
        LOG_INFO << "fd = " << chl->fd() << " is closed";
        _loop->removeChannel(chl);
    }
}
