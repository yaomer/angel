#ifndef _ANGEL_ECHOCLIENT_H
#define _ANGEL_ECHOCLIENT_H

#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>
#include <Angel/LogStream.h>

using namespace Angel;
using std::placeholders::_1;
using std::placeholders::_2;

class EchoClient {
public:
    EchoClient(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr, "EchoClient")
    {
        _client.setMessageCb(
                std::bind(&EchoClient::onMessage, this, _1, _2));
    }
    void write(char *buf)
    {
        ssize_t n = ::write(_client.conn()->getChannel()->fd(), 
                buf, strlen(buf));
        if (n < 0)
            logError("write: %s", strerrno());
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
    {
        std::cout << buf.c_str() << std::endl;
        buf.retrieveAll();
    }
    void start() { _client.start(); }
    void quit() { _loop->quit(); }
private:
    EventLoop *_loop;
    TcpClient _client;
};

#endif
