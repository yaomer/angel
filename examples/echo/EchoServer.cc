#include "../Angel.h"
#include "EchoServer.h"

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    EchoServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
