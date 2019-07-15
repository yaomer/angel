#ifndef _ANGEL_CHARGEN_CLIENT_H
#define _ANGEL_CHARGEN_CLIENT_H

#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

using namespace Angel;
using std::placeholders::_1;
using std::placeholders::_2;

class ChargenClient {
public:
    ChargenClient(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr, "ChargenClient")
    {
        _client.setMessageCb(
                std::bind(&ChargenClient::onMessage, this, _1, _2));
    }
    void onMessage(const TcpConnectionPtr &conn, Buffer& buf)
    {
        std::cout << buf.c_str();
        buf.retrieveAll();
    }
    void start() { _client.start(); }
    void quit() { _loop->quit(); }
private:
    EventLoop *_loop;
    TcpClient _client;
};

#endif // _ANGEL_CHARGEN_CLIENT_H
