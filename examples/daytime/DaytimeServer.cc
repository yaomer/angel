#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

// daytime协议：详见RFC867
// 格式：WeekDay, Month Day, Year Time-Zone

// 可用telnet充当客户端

int main()
{
    Angel::EventLoop loop;
    Angel::TcpServer DaytimeServer(&loop, Angel::InetAddr(8000));
    DaytimeServer.setConnectionCb([](const Angel::TcpConnectionPtr& conn){
            struct tm tm;
            time_t now = time(nullptr);
            localtime_r(&now, &tm);
            char buf[64];
            strftime(buf, sizeof(buf), "%a, %b %d, %Y %X-%Z\n", &tm);
            conn->send(buf);
            conn->close();
            });
    DaytimeServer.start();
    loop.run();
}
