#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include <iostream>

#include "HttpContext.h"

void onMessage(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf)
{
    if (!conn->getContext().has_value()) return;
    auto& context = std::any_cast<HttpContext&>(conn->getContext());
    while (buf.readable() >= 2) {
        int crlf = buf.findCrlf();
        if (crlf < 0) break;
        switch (context.state()) {
        case HttpContext::PARSE_LINE:
            context.parseLine(buf.peek(), buf.peek() + crlf + 2);
            break;
        case HttpContext::PARSE_HEADER:
            context.parseHeader(buf.peek(), buf.peek() + crlf + 2);
            if (context.state() == HttpContext::PARSE_LINE)
                context.response(conn);
            break;
        }
        buf.retrieve(crlf + 2);
    }
}

int main()
{
    Angel::EventLoop loop;
    Angel::TcpServer HttpServer(&loop, Angel::InetAddr(8000));
    HttpServer.setConnectionCb([](const Angel::TcpConnectionPtr& conn){
            conn->setContext(HttpContext());
            });
    HttpServer.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            onMessage(conn, buf);
            });
    std::cout << "Server is running at localhost:8000" << std::endl;
    HttpServer.start();
    loop.run();
}
