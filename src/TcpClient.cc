#include "TcpClient.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Connector.h"
#include "Channel.h"

using namespace Angel;

TcpClient::TcpClient(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _connector(loop, inetAddr)
{

}

TcpClient::~TcpClient()
{

}

void TcpClient::listenStdin()
{
    Channel *sin = new Channel(_loop);
    sin->setFd(0);
    sin->setReadCb([this]{ this->readStdin(); });
    _loop->addChannel(sin);
}

void TcpClient::readStdin()
{
    Buffer buf;
    ssize_t n = buf.readFd(0);
    if (n > 0) {
        if (_stdinCb) {
            _stdinCb(_connector.channel(), buf);
        }
    } else if (n == 0) {
        _connector.handleClose();
    } else {
        _connector.handleError();
    }
}

void TcpClient::start()
{
    _connector.connect();
}

void TcpClient::quit()
{
    _loop->quit();
}
