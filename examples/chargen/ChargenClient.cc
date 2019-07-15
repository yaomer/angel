#include <Angel/EventLoop.h>
#include <Angel/InetAddr.h>
#include <Angel/TcpClient.h>
#include "ChargenClient.h"

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr connAddr(8000, "127.0.0.1");
    ChargenClient client(&loop, connAddr);
    client.start();
    loop.run();
}
