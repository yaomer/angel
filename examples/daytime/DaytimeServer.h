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
        const char *time = TimeStamp::timeStr(TimeStamp::LOCAL_TIME);
        conn->send(time);
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
