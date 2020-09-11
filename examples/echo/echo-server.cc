#include <angel/server.h>

// echo协议：详见RFC862
// 服务端回射客户端的数据

int main()
{
    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    serv.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            conn->send(buf.peek(), buf.readable());
            buf.retrieve_all();
            });
    serv.start();
    loop.run();
}
