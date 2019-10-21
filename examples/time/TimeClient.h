#ifndef _ANGEL_TIME_CLIENT_H
#define _ANGEL_TIME_CLIENT_H

#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

#include <iostream>

using std::placeholders::_1;
using std::placeholders::_2;

class TimeClient {
public:
    TimeClient(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr)
    {
        _client.setMessageCb(
                std::bind(&TimeClient::onMessage, this, _1, _2));
    }
    void onMessage(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf)
    {
        if (buf.readable() >= sizeof(int32_t)) {
            char time[32];
            int32_t tm32 = *reinterpret_cast<int32_t*>(buf.peek());
            time_t tm = static_cast<time_t>(tm32);
            std::cout << ctime_r(&tm, time);
            buf.retrieve(sizeof(int32_t));
        }
    }
    void start() { _client.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpClient _client;
};

#endif // _ANGEL_TIME_CLIENT_H
