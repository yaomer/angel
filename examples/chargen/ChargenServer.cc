#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include "ChargenServer.h"

// chargen协议：详见RFC864
// 连接建立后，服务端不断发送任意字符序列到客户端，
// 直到客户端关闭连接

using namespace Angel;

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    ChargenServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
