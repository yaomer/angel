#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

#include <iostream>

int main()
{
    Angel::EventLoop loop;
    Angel::TcpClient ChargenClient(&loop, Angel::InetAddr(8000, "127.0.0.1"));
    ChargenClient.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            std::cout << buf.c_str();
            buf.retrieveAll();
            });
    ChargenClient.start();
    loop.run();
}
