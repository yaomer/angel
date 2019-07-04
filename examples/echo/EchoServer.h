#ifndef _ANGEL_ECHOSERVER_H
#define _ANGEL_ECHOSERVER_H

#include "../Angel.h"

using namespace Angel;

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
        std::string msg(buf.c_str());
        conn->send(msg);
        buf.retrieveAll();
    }
    void start()
    {
        _server.setThreadNums(6);
        _server.start();
    }
    void quit()
    {
        _loop->quit();
    }
private:
    EventLoop *_loop;
    TcpServer _server;
};

#endif
