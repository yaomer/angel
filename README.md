### 什么是Angel 
Angel是一个C++11风格的适用于Linux和Mac OS平台的多线程网络库。

### 为什么要选择Angel 
+ **支持多种网络模型，可在程序中动态配置**
    + 单线程Reactor，接收和处理连接都在主线程
    + 单线程Reactor + 线程池，对于耗时任务可以开启一个任务线程池去处理
    + 多线程Reactor，主从Reactor架构，主线程负责接收新连接，然后分发给一个I/O线程池中的线程
    + 多线程Reactor + 线程池，上面两种模型的结合
+ **提供了一个高效易用的定时器**
    + 利用I/O多路复用的超时参数，通过红黑树管理注册的超时事件，我们实现了一个高效的定时器，并且提供了非常便于使用的接口。而且它与其他模块没有依赖，你可以很容易将它嵌入到你的项目中。
+ **支持信号处理**
    + 我们将信号处理也统一到了I/O多路复用之中，可以很方便的处理你想处理的信号，包括注册对应信号的handler和动态取消注册的handler。
+ **提供了一个高性能异步日志模块**
    + 我们提供了不同的记录等级（DEBUG、INFO、WARN、ERROR），并且可以在程序中动态修改日志记录等级。因为采用了前后端分离的架构，所以前端可以无阻塞的大量写入日志，而后端通过利用双缓冲机制最大限度的减少持有锁的时间，保证了前端的低延迟。日志模块也是独立的，可以很容易分离出来。
+ **支持客户端超时处理**
    + 用户可以设置一个回调，服务器会在空闲客户连接超时后调用该回调。
+ **支持多种I/O多路复用机制**
    + 支持select、poll、epoll以及kqueue。在编译时Angel会根据系统选择最高效的多路复用机制，当然也可以手工选择想要的多路复用机制。
+ **使用方便，可以快速编写TCP网络程序的服务器和客户端**
    + 通过使用我们提供的TcpServer和TcpClient两个类，用户只需要填上对应的消息回调就可实现不同的服务器程序和客户端。
+ **高性能，且没有额外依赖**
    + Angel完全是自包含的，无需安装任何外部依赖库。

### 安装
你可以通过以下命令来完成Angel的安装：
```
$ git clone https://github.com/yaomer/Angel
```
在正式编译之前，你需要检查你的CMake和编译器的版本是否满足要求：
```
CMake >= 3.1
gcc >= 7.0 / clang >= 4.0
```
然后进入到你安装的Angel的主目录中，运行build.sh即可完成编译，注意该脚本需要将生成的静态库libangel.a和头文件写入到系统目录，所以可能需要root权限。

### 用法
下面程序示例都需要包含以下头文件：
```cpp
#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>
#include <Angel/TcpClient.h>
#include <Angel/EventLoopThread.h>
#include <Angel/LogStream.h>
#include <iostream>
```
##### 定时器的使用
```cpp
int main(void)
{
    Angel::EventLoop loop;
    // 3s后打印出"hello, world"
    loop.runAfter(3000, []{ std::cout << "hello, world\n"; });
    // 一个计数器，每隔1s加1
    int count = 1;
    loop.runEvery(1000, [&count]() mutable { std::cout << count++ << "\n"; });
    loop.run();
    return 0;
}
```
##### 信号处理
```cpp
int main(void)
{
    // 信号处理需要EventLoop驱动
    Angel::EventLoop loop;
    // 按两次Ctr+C才可退出程序
    Angel::addSignal(SIGINT, []{
            std::cout << "SIGINT is triggered\n";
            // 恢复信号原语义，使用addSignal(SIGINT, nullptr)可忽略SIGINT信号
            Angel::cancelSignal(SIGINT);
            });
    loop.run();
    return 0;
}
```
##### 一个简单的Echo服务器与客户端
```cpp
int main(void)
{
    Angel::EventLoop loop;
    Angel::TcpServer server(&loop, Angel::InetAddr(8888));
    // 当接收到客户端发来的消息时会调用这个函数
    server.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            conn->formatSend("hello: %s", buf.c_str());
            buf.retrieveAll();
            });
    server.start();
    loop.run();
    return 0;
}
```
```cpp
int main(void)
{
    // loop线程监听服务端的响应
    Angel::EventLoopThread t_loop;
    Angel::TcpClient client(t_loop.getLoop(), Angel::InetAddr(8000, "127.0.0.1"));
    client.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            std::cout << buf.c_str() << std::endl;
            buf.retrieveAll();
            });
    client.start();
    // 主线程监听键盘输入
    char buf[1024];
    while (::fgets(buf, sizeof(buf), stdin)) {
        buf[strlen(buf) - 1] = '\0';
        client.conn()->send(buf);
    }
    return 0;
}
```
##### 将服务器多线程化
```cpp
int main(void)
{
    Angel::EventLoop loop;
    Angel::TcpServer server(&loop, Angel::InetAddr(8888));
    server.setMessageCb([](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            conn->formatSend("hello: %s", buf.c_str());
            buf.retrieveAll();
            });
    // 主线程接收新连接，4个io线程处理连接
    server.setIoThreadNums(4);
    server.start();
    loop.run();
    return 0;
}
```
##### 使用一个线程池处理任务
```cpp
int main(void)
{
    Angel::EventLoop loop;
    Angel::TcpServer server(&loop, Angel::InetAddr(8888));
    server.setMessageCb([&server](const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf){
            // 这里必须拷贝一份conn，因为离开这个函数后conn就会被析构
            server.executor([conn, &buf]{
                    conn->formatSend("hello: %s", buf.c_str());
                    buf.retrieveAll();
                    });
            });
    // 开启一个拥有4个线程的任务线程池
    server.setTaskThreadNums(4);
    server.start();
    loop.run();
    return 0;
}
```
##### 日志记录与控制
```cpp
int main(void)
{
    // 日志默认输出到文件中，存储于当前目录下的.log目录中
    // FLUSH_TO_STDOUT表示将日志打印到标准输出
    Angel::setLoggerFlush(Angel::Logger::FLUSH_TO_STDOUT);
    // 只打印日志级别大于等于WARN的日志，默认打印所有级别日志
    Angel::setLoggerLevel(Angel::Logger::WARN);
    logInfo("hello");
    logWarn("hello");
    logError("hello");
    return 0;
}
```
### 性能测试
使用示例中简陋的[HTTP服务器](https://github.com/yaomer/Angel/tree/master/examples/http)与Nginx作简单的性能对比测试

我的测试环境如下：
```
系统：MAC OS Mojave 10.14
处理器：Intel Core i5 双核四线程 1.6GHz
编译器：Apple LLVM version 10.0.1
```
使用ab进行测试，angel-http是单线程，nginx/1.15.8开一个工作进程
```
ab -c [clients] -n 100000 [url]; clients = { 1, 10, 100, 200, 500, 1000 }
```
每种情况测3次，取平均值，下面是测试结果绘制的折线图

![](https://github.com/yaomer/pictures/blob/master/webbench.png?raw=true)

### 后续优化与展望
+ 考虑使用REUSEPORT，由内核作负载均衡
+ 考虑加入多进程模式，为用户提供更多选择
