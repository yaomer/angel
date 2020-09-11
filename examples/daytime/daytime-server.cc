#include <angel/server.h>

// daytime协议：详见RFC867
// 格式：WeekDay, Month Day, Year Time-Zone

// 可用telnet充当客户端

int main()
{
    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    serv.set_connection_handler([](const angel::connection_ptr& conn){
            struct tm tm;
            time_t now = time(nullptr);
            localtime_r(&now, &tm);
            char buf[64];
            strftime(buf, sizeof(buf), "%a, %b %d, %Y %X-%Z\n", &tm);
            conn->send(buf);
            conn->close();
            });
    serv.start();
    loop.run();
}
