#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

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

    Angel::EventLoop loop;
    Angel::TcpServer ChargenServer(&loop, Angel::InetAddr(8000));
    ChargenServer.setConnectionCb([&message](const Angel::TcpConnectionPtr& conn){
            conn->send(message);
            });
    ChargenServer.setWriteCompleteCb([&message](const Angel::TcpConnectionPtr& conn){
            conn->send(message);
            });
    ChargenServer.start();
    loop.run();
}
