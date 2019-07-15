#ifndef _ANGEL_ECHOSERVER_H
#define _ANGEL_ECHOSERVER_H

#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

using namespace Angel;
using std::placeholders::_1;
using std::placeholders::_2;

class EchoServer {
public:
    EchoServer(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setMessageCb(
                std::bind(&EchoServer::onMessage, this, _1, _2));
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
    {
        conn->send(buf.peek(), buf.readable());
        buf.retrieveAll();
    }
    void start() { _server.start(); }
    void quit() { _loop->quit(); }
private:
    EventLoop *_loop;
    TcpServer _server;
};

#endif
