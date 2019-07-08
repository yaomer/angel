#ifndef _ANGEL_DISCARD_SERVER_H
#define _ANGEL_DISCARD_SERVER_H

#include "../Angel.h"
#include <iostream>
#include <mutex>

using namespace Angel;

class DiscardServer {
public:
    DiscardServer(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setMessageCb(
                std::bind(&DiscardServer::onMessage, this, _1, _2));
        // 统计每秒的流量
        _loop->runEvery(1000, [this]{ this->printFlow(); });
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
    {
        // 对于目前是单线程的server来说是可以不用加锁的
        std::lock_guard<std::mutex> mlock(_mutex);
        _recvBytes += buf.readable();
        buf.retrieveAll();
    }
    void printFlow()
    {
        std::lock_guard<std::mutex> mlock(_mutex);
        if (_recvBytes > 0) {
            std::cout << "flow per second: " << _recvBytes << std::endl;
            _recvBytes = 0;
        }
    }
    void start() { _server.start(); }
private:
    EventLoop *_loop;
    TcpServer _server;
    std::mutex _mutex;
    size_t _recvBytes;
};

#endif // _ANGEL_DISCARD_SERVER_H
