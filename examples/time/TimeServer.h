#ifndef _ANGEL_TIME_SERVER_H
#define _ANGEL_TIME_SERVER_H

#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

using std::placeholders::_1;

class TimeServer {
public:
    TimeServer(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setConnectionCb(
                std::bind(&TimeServer::onConnection, this, _1));
    }
    void onConnection(const Angel::TcpConnectionPtr& conn)
    {
        int32_t tm = static_cast<int32_t>(time(nullptr));
        conn->send(reinterpret_cast<void*>(&tm), sizeof(int32_t));
        conn->close();
    }
    void start() { _server.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpServer _server;
};

#endif // _ANGEL_TIME_SERVER_H
