#include <angel/server.h>

#include <mutex>

// discard协议：详见RFC863
// 服务端丢弃客户端发送的所有数据

int main()
{
    std::mutex mutex;
    size_t recv_bytes = 0;
    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    serv.set_message_handler([&mutex, &recv_bytes]
            (const angel::connection_ptr& conn, angel::buffer& buf){
            // 对于目前是单线程的server来说是可以不用加锁的
            std::lock_guard<std::mutex> mlock(mutex);
            recv_bytes += buf.readable();
            buf.retrieve_all();
            });
    // 统计每秒的流量
    loop.run_every(1000, [&mutex, &recv_bytes]{
            std::lock_guard<std::mutex> mlock(mutex);
            if (recv_bytes > 0) {
                printf("flow per second: %.2fMB\n", recv_bytes*1.0/(1024*1024));
                recv_bytes = 0;
            }
            });
    serv.start();
    loop.run();
}
