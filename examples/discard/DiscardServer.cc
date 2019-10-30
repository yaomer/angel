#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

#include <iostream>
#include <mutex>

// discard协议：详见RFC863
// 服务端丢弃客户端发送的所有数据

int main()
{
    std::mutex mutex;
    size_t recvBytes = 0;
    Angel::EventLoop loop;
    Angel::TcpServer DiscardServer(&loop, Angel::InetAddr(8000));
    DiscardServer.setMessageCb([&mutex, &recvBytes]
            (const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            // 对于目前是单线程的server来说是可以不用加锁的
            std::lock_guard<std::mutex> mlock(mutex);
            recvBytes += buf.readable();
            buf.retrieveAll();
            });
    // 统计每秒的流量
    loop.runEvery(1000, [&mutex, &recvBytes]{
            std::lock_guard<std::mutex> mlock(mutex);
            if (recvBytes > 0) {
                std::cout << "flow per second: " << recvBytes << std::endl;
                recvBytes = 0;
            }
            });
    DiscardServer.start();
    loop.run();
}
