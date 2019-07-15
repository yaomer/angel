#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include "ChatServer.h"

using namespace Angel;

// 用telnet充当client，可以多人匿名聊天

const char *ChatServer::_help = {
    "usage:\n"
    "-------------------------------------------------------------------------\n"
    "|id num [msg]| send [msg] to [id num]                                   |\n"
    "|grp [msg] | broadcast [msg] to all clients                             |\n"
    "|[msg]     | if ([last-id] is null) echo [msg]; else id [last-id] [msg] |\n"
    "-------------------------------------------------------------------------\n"
};

int main()
{
    EventLoop loop;
    InetAddr listenAddr(8000);
    ChatServer server(&loop, listenAddr);
    server.start();
    loop.run();
}
