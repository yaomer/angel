#include "DiscardServer.h"
#include "../Angel.h"

// discard协议：详见RFC863
// 服务端丢弃客户端发送的所有数据

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    DiscardServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
