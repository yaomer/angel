#ifndef _ANGEL_ECHOCLIENT_H
#define _ANGEL_ECHOCLIENT_H

#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

using std::placeholders::_1;
using std::placeholders::_2;

class EchoClient {
public:
    EchoClient(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr, "EchoClient")
    {
        _client.setMessageCb(
                std::bind(&EchoClient::onMessage, this, _1, _2));
    }
    void onMessage(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf)
    {
        std::cout << buf.c_str() << std::endl;
        buf.retrieveAll();
    }
    void send(char *buf)
    {
        _client.conn()->send(buf);
    }
    void start() { _client.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpClient _client;
};

#endif
