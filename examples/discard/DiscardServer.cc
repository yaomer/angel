#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include "DiscardServer.h"

// discard协议：详见RFC863
// 服务端丢弃客户端发送的所有数据

int main()
{
    Angel::EventLoop loop;
    Angel::InetAddr listenAddr(8000);
    DiscardServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
