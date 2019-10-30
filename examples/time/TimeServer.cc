#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

// time协议：详见RFC868
// 服务器返回一个32位整数，表示从UTC时间至今的秒数

int main()
{
    Angel::EventLoop loop;
    Angel::TcpServer TimeServer(&loop, Angel::InetAddr(8000));
    TimeServer.setConnectionCb([](const Angel::TcpConnectionPtr& conn){
            int32_t tm = static_cast<int32_t>(time(nullptr));
            conn->send(reinterpret_cast<void*>(&tm), sizeof(int32_t));
            conn->close();
            });
    TimeServer.start();
    loop.run();
}
