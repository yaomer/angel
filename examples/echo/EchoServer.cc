#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

// echo协议：详见RFC862
// 服务端回射客户端的数据

int main()
{
    Angel::EventLoop loop;
    Angel::TcpServer EchoServer(&loop, Angel::InetAddr(8000));
    EchoServer.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            conn->send(buf.peek(), buf.readable());
            buf.retrieveAll();
            });
    EchoServer.start();
    loop.run();
}
