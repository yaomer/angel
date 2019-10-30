#include <Angel/EventLoopThread.h>
#include <Angel/TcpClient.h>

#include <iostream>

int main()
{
    Angel::EventLoopThread t_loop;
    Angel::TcpClient EchoClient(t_loop.getLoop(), Angel::InetAddr(8000, "127.0.0.1"));
    EchoClient.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            std::cout << buf.c_str() << std::endl;
            buf.retrieveAll();
            });
    EchoClient.start();

    char buf[1024];
    while (::fgets(buf, sizeof(buf), stdin)) {
        buf[strlen(buf) - 1] = '\0';
        EchoClient.conn()->send(buf);
    }
}
