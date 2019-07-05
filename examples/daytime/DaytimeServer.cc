#include "DaytimeServer.h"
#include "../Angel.h"

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    DaytimeServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
