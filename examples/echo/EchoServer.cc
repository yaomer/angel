#include <Angel/TcpServer.h>
#include "EchoServer.h"

// echo协议：详见RFC862
// 服务端回射客户端的数据

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    EchoServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
