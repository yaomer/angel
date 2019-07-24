#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include "FibServer.h"

// 这里我们用telnet充当client来测试FibServer
// 在本机上测试如下：
// $ telnet localhost 8000
// $ 42
// $ 267914296

int main()
{
    Angel::EventLoop loop;
    Angel::InetAddr listenAddr(8000);
    FibServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
