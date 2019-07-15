#include <Angel/TcpServer.h>
#include "EchoClient.h"

using namespace Angel;

int main()
{
    EventLoopThread t_loop;
    InetAddr connAddr(8000, "127.0.0.1");
    EchoClient client(t_loop.getLoop(), connAddr);
    client.start();

    char buf[1024];
    while (::fgets(buf, sizeof(buf), stdin)) {
        buf[strlen(buf) - 1] = '\0';
        client.write(buf);
    }
}
