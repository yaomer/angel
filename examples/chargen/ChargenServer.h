#ifndef _ANGEL_CHARGEN_SERVER_H
#define _ANGEL_CHARGEN_SERVER_H

#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include <string>

using std::placeholders::_1;

class ChargenServer {
public:
    ChargenServer(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setConnectionCb(
                std::bind(&ChargenServer::onConnection, this, _1));
        _server.setWriteCompleteCb(
                std::bind(&ChargenServer::onWriteComplete, this, _1));
        _server.setIoThreadNums(3);
        creatChargenMsg();
    }
    void onConnection(const Angel::TcpConnectionPtr& conn)
    {
        conn->send(_msg);
    }
    void onWriteComplete(const Angel::TcpConnectionPtr& conn)
    {
        conn->send(_msg);
    }
    void creatChargenMsg()
    {
        std::string s;
        // 33 ~ 127是可打印字符
        for (int i = 33; i < 127; i++)
            s.push_back(i);
        s += s;
        // 每行72个字符，一组数据有95行
        for (int i = 0; i < 127 - 33; i++)
            _msg += s.substr(i, 72) + "\n";
    }
    void start() { _server.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpServer _server;
    std::string _msg;
};

#endif // _ANGEL_CHARGEN_SERVER_H
