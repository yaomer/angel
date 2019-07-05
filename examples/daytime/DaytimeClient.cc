#include "DaytimeClient.h"
#include "../Angel.h"

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr connAddr(8000, "127.0.0.1");
    DaytimeClient client(&loop, connAddr);
    client.start();
    loop.run();
}
