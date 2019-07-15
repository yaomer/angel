#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>
#include "DaytimeClient.h"

// daytime协议：详见RFC867
// 格式：WeekDay, Month Day, Year Time-Zone

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr connAddr(8000, "127.0.0.1");
    DaytimeClient client(&loop, connAddr);
    client.start();
    loop.run();
}
