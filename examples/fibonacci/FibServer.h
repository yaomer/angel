#ifndef _ANGEL_FIBSERVER_H
#define _ANGEL_FIBSERVER_H

#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

using namespace Angel;
using std::placeholders::_1;
using std::placeholders::_2;

class FibServer {
public:
    FibServer(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setMessageCb(
                std::bind(&FibServer::onMessage, this, _1, _2));
        // 用一个thread pool来计算fibonacci
        _server.setTaskThreadNums(4);
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
    {
        size_t n = atol(buf.c_str());
        _server.executor([this, n, conn]{ 
                char s[64];
                snprintf(s, sizeof(s), "%zu\n", this->fib(n));
                conn->send(s);
                });
        buf.retrieveAll();
    }
    // 递归方法显而易见是相当耗时的，这里我们用它只是为了用到thread pool，
    // 此时直接在io线程计算显然是不合适的
    size_t fib(size_t n)
    {
        if (n == 0 || n == 1)
            return n;
        return fib(n - 1) + fib(n - 2);
    }
    void start() { _server.start(); }
    void quit() { _loop->quit(); }
private:
    EventLoop *_loop;
    TcpServer _server;
};

#endif // _ANGEL_FIBSERVER_H
