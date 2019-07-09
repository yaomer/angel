#ifndef _ANGEL_TIME_SERVER_H
#define _ANGEL_TIME_SERVER_H

#include "../Angel.h"

using namespace Angel;

class TimeServer {
public:
    TimeServer(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setConnectionCb(
                std::bind(&TimeServer::onConnection, this, _1));
    }
    void onConnection(const TcpConnectionPtr& conn)
    {
        int32_t tm = static_cast<int32_t>(time(nullptr));
        conn->send(reinterpret_cast<void*>(&tm), sizeof(int32_t));
        conn->close();
    }
    void start() { _server.start(); }
    void quit() { _loop->quit(); }
private:
    EventLoop *_loop;
    TcpServer _server;
};

#endif // _ANGEL_TIME_SERVER_H
