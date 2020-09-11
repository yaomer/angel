#include <angel/server.h>

#include <string>

// chargen协议：详见RFC864
// 连接建立后，服务端不断发送任意字符序列到客户端，
// 直到客户端关闭连接

int main()
{
    // 生成字符序列
    std::string message, s;
    // 33 ~ 127是可打印字符
    for (int i = 33; i < 127; i++)
        s.push_back(i);
    s += s;
    // 每行72个字符，一组数据有95行
    for (int i = 0; i < 127 - 33; i++)
        message += s.substr(i, 72) + "\n";

    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    serv.set_connection_handler([&message](const angel::connection_ptr& conn){
            conn->send(message);
            });
    serv.set_write_complete_handler([&message](const angel::connection_ptr& conn){
            conn->send(message);
            });
    serv.start();
    loop.run();
}
