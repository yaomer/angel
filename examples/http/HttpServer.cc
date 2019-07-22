#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include <iostream>
#include "HttpServer.h"

int main()
{
    Angel::EventLoop loop;
    Angel::InetAddr listenAddr(8000);
    HttpServer server(&loop, listenAddr);
    std::cout << "Server is running at localhost:8000" << std::endl;
    server.start();
    loop.run();
}
