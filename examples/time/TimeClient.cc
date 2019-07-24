#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>
#include "TimeClient.h"

int main()
{
    Angel::EventLoop loop;
    Angel::InetAddr connAddr(8000, "127.0.0.1");
    TimeClient client(&loop, connAddr);
    client.start();
    loop.run();
}
