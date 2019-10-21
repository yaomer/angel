#ifndef _ANGEL_DAYTIMECLIENT_H
#define _ANGEL_DAYTIMECLIENT_H

#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

#include <iostream>

using std::placeholders::_1;
using std::placeholders::_2;

class DaytimeClient {
public:
    DaytimeClient(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr)
    {
        _client.setMessageCb(
                std::bind(&DaytimeClient::onMessage, this, _1, _2));
    }
    void onMessage(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf)
    {
        std::cout << buf.c_str();
        buf.retrieveAll();
    }
    void start() { _client.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpClient _client;
};

#endif
