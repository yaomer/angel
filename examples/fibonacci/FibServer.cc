#include "FibServer.h"
#include "../Angel.h"

// 这里我们用telnet充当client来测试FibServer
// 在本机上测试如下：
// $ telnet localhost 8000
// $ 42
// $ 267914296

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    FibServer server(&loop, listenAddr);
    server.start();
    loop.run();
}