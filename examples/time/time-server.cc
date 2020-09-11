#include <angel/server.h>

// time协议：详见RFC868
// 服务器返回一个32位整数，表示从UTC时间至今的秒数

int main()
{
    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    serv.set_connection_handler([](const angel::connection_ptr& conn){
            int32_t tm = static_cast<int32_t>(time(nullptr));
            conn->send(reinterpret_cast<void*>(&tm), sizeof(int32_t));
            conn->close();
            });
    serv.start();
    loop.run();
}
