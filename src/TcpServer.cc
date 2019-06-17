#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"

using namespace Angel;

TcpServer::TcpServer(EventLoop *loop, InetAddr& inetAddr)
    : _loop(loop),
    _acceptor(loop, inetAddr)
{

}

TcpServer::~TcpServer()
{

}

void TcpServer::start()
{
    _acceptor.listen();
}

void TcpServer::quit()
{
    _loop->quit();
    _loop->wakeup();
}
