#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>
#include "DaytimeClient.h"

// daytime协议：详见RFC867
// 格式：WeekDay, Month Day, Year Time-Zone

int main()
{
    Angel::EventLoop loop;
    Angel::InetAddr connAddr(8000, "127.0.0.1");
    DaytimeClient client(&loop, connAddr);
    client.start();
    loop.run();
}
