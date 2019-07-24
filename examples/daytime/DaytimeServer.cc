#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include "DaytimeServer.h"

int main()
{
    Angel::EventLoop loop;
    Angel::InetAddr listenAddr(8000);
    DaytimeServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
