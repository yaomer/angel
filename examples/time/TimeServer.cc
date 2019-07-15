#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include "TimeServer.h"

// time协议：详见RFC868
// 服务器返回一个32位整数，表示从UTC时间至今的秒数

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    TimeServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
