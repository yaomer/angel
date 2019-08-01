#ifndef _ANGEL_HTTP_SERVER_H
#define _ANGEL_HTTP_SERVER_H

#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include "HttpContext.h"

using std::placeholders::_1;
using std::placeholders::_2;

class HttpServer {
public:
    HttpServer(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setConnectionCb(
                std::bind(&HttpServer::onConnection, this, _1));
        _server.setMessageCb(
                std::bind(&HttpServer::onMessage, this, _1, _2));
    }
    void onConnection(const Angel::TcpConnectionPtr& conn)
    {
        conn->setContext(HttpContext());
    }
    void onMessage(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf)
    {
        auto& context = std::any_cast<HttpContext&>(conn->getContext());
        std::cout << buf.c_str();
        while (buf.readable() >= 2) {
            int crlf = buf.findCrlf();
            if (crlf >= 0) {
                switch (context.state()) {
                case HttpContext::PARSE_LINE:
                    context.parseReqLine(buf.peek(), buf.peek() + crlf + 2);
                    break;
                case HttpContext::PARSE_HEADER:
                    context.parseReqHeader(buf.peek(), buf.peek() + crlf + 2);
                    context.response(conn);
                    break;
                }
                buf.retrieve(crlf + 2);
            } else
                break;
        }
    }
    void start() { _server.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpServer _server;
};

#endif // _ANGEL_HTTP_SERVER_H
