#ifndef _ANGEL_DAYTIMECLIENT_H
#define _ANGEL_DAYTIMECLIENT_H

#include "../Angel.h"

using namespace Angel;

class DaytimeClient {
public:
    DaytimeClient(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr)
    {
        _client.setMessageCb(
                std::bind(&DaytimeClient::onMessage, this, _1, _2));
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
    {
        std::cout << buf.c_str();
        buf.retrieveAll();
    }
    void start()
    {
        _client.start();
    }
    void quit()
    {
        _loop->quit();
    }
private:
    EventLoop *_loop;
    TcpClient _client;
};

#endif
