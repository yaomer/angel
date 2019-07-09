#ifndef _ANGEL_TIME_CLIENT_H
#define _ANGEL_TIME_CLIENT_H

#include "../Angel.h"

using namespace Angel;

class TimeClient {
public:
    TimeClient(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr, "TimeClient")
    {
        _client.setMessageCb(
                std::bind(&TimeClient::onMessage, this, _1, _2));
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
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
    void quit() { _loop->quit(); }
private:
    EventLoop *_loop;
    TcpClient _client;
};

#endif // _ANGEL_TIME_CLIENT_H
