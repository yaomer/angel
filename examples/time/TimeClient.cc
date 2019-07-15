#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>
#include "TimeClient.h"

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr connAddr(8000, "127.0.0.1");
    TimeClient client(&loop, connAddr);
    client.start();
    loop.run();
}
