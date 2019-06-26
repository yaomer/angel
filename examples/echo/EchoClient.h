#ifndef _ANGEL_ECHOCLIENT_H
#define _ANGEL_ECHOCLIENT_H

#include "../Angel.h"

using namespace Angel;

class EchoClient {
public:
    EchoClient(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr)
    {
        _client.setMessageCb(
                std::bind(&EchoClient::onMessage, this, _1, _2));
    }
    void start()
    {
        _client.start();
    }
    void write(char *buf)
    {
        ::write(_client.conn()->getChannel()->fd(), buf, strlen(buf));
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
    {
        std::cout << buf.c_str() << std::endl;
        buf.retrieveAll();
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
