#ifndef _ANGEL_CHARGEN_CLIENT_H
#define _ANGEL_CHARGEN_CLIENT_H

#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

using std::placeholders::_1;
using std::placeholders::_2;

class ChargenClient {
public:
    ChargenClient(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr, "ChargenClient")
    {
        _client.setMessageCb(
                std::bind(&ChargenClient::onMessage, this, _1, _2));
    }
    void onMessage(const Angel::TcpConnectionPtr &conn, Angel::Buffer& buf)
    {
        std::cout << buf.c_str();
        buf.retrieveAll();
    }
    void start() { _client.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpClient _client;
};

#endif // _ANGEL_CHARGEN_CLIENT_H
