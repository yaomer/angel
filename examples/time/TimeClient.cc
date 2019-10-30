#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

#include <iostream>
#include <time.h>

int main()
{
    Angel::EventLoop loop;
    Angel::TcpClient TimeClient(&loop, Angel::InetAddr(8000, "127.0.0.1"));
    TimeClient.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            if (buf.readable() >= sizeof(int32_t)) {
                char time[32];
                int32_t tm32 = *reinterpret_cast<int32_t*>(buf.peek());
                time_t tm = static_cast<time_t>(tm32);
                std::cout << ctime_r(&tm, time);
                buf.retrieve(sizeof(int32_t));
            }
            });
    TimeClient.start();
    loop.run();
}
