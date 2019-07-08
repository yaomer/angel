#ifndef _ANGEL_DAYTIMESERVER_H
#define _ANGEL_DAYTIMESERVER_H

#include "../Angel.h"

using namespace Angel;

class DaytimeServer {
public:
    DaytimeServer(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setConnectionCb(
                std::bind(&DaytimeServer::onConnection, this, _1));
    }
    void onConnection(const TcpConnectionPtr& conn)
    {
        struct tm tm;
        time_t now = time(nullptr);
        localtime_r(&now, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), "%a, %b %d, %Y %X-%Z\n", &tm);
        conn->send(buf);
        conn->close();
    }
    void start()
    {
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

#endif // _ANGEL_DAYTIMESERVER_H
