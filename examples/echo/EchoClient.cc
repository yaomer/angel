#include <Angel/EventLoopThread.h>
#include <Angel/TcpClient.h>
#include "EchoClient.h"

int main()
{
    Angel::EventLoopThread t_loop;
    Angel::InetAddr connAddr(8000, "127.0.0.1");
    EchoClient client(t_loop.getLoop(), connAddr);
    client.start();

    char buf[1024];
    while (::fgets(buf, sizeof(buf), stdin)) {
        buf[strlen(buf) - 1] = '\0';
        client.send(buf);
    }
}
