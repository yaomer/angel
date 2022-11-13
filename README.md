```
                        _
  __ _ _ __   __ _  ___| |
 / _` | '_ \ / _` |/ _ \ |
| (_| | | | | (_| |  __/ |
 \__,_|_| |_|\__, |\___|_|
             |___/
```
### 介绍 
angel是一个C++11风格的适用于Linux和Mac OS平台的多线程网络库。

### 安装
你可以通过以下命令来完成angel的安装：
```
$ git clone https://github.com/yaomer/angel
```
在正式编译之前，你需要检查你的CMake和编译器的版本是否满足要求：
```
CMake >= 3.1
gcc >= 7.0 / clang >= 4.0
```
然后进入到你安装的angel的主目录中，运行build.sh即可完成编译，注意该脚本需要将生成的静态库libangel.a和头文件写入到系统目录，所以可能需要root权限。
```
$ ./build.sh --install-prefix=path # 库文件的安装路径，默认为/usr/local 
$ ./build.sh --with-ssl            # 带有SSL支持，需要你预先安装好OpenSSL(>=1.1.0)
$ ./build.sh --release             # 编译生成Release版本
$ ./build.sh --debug               # 编译生成Debug版本
```

### 用法
下面程序示例需要包含以下头文件：
```cpp
#include <angel/server.h>
#include <angel/client.h>
#include <angel/evloop_thread.h>
#include <iostream>
```
然后在编译时链接至`angel`
```
$ clang++ -std=c++17 test.cc -langel
```

#### 定时器的使用
```cpp
int main(void)
{
    angel::evloop loop;
    // 3s后打印出"hello, world"
    loop.run_after(3000, []{ std::cout << "hello, world\n"; });
    // 一个计数器，每隔1s加1
    int count = 1;
    loop.run_every(1000, [&count]() mutable { std::cout << count++ << "\n"; });
    loop.run();
}
```
#### 一个简单的Echo服务器与客户端
```cpp
int main(void)
{
    angel::evloop loop;
    // 服务器监听端口8888
    angel::server server(&loop, angel::inet_addr(8888));
    // 当接收到客户端发来的消息时会调用这个函数
    server.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            conn->format_send("hello: %s", buf.c_str());
            buf.retrieve_all();
            });
    server.start();
    loop.run();
}
```
我们有两种方法可用于处理标准输入：
1. 使用双线程，主线程阻塞等待键盘输入，另起一个线程负责监听服务端响应。
2. 只用单线程，使用channel将标准输入响应事件注册入evloop中，同时监听标准输入和服务端响应。

```cpp
int main(void)
{
    // loop线程监听服务端的响应
    angel::evloop_thread t_loop;
    angel::client client(t_loop.wait_loop(), angel::inet_addr("127.0.0.1", 8888));
    client.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            std::cout << buf.c_str() << std::endl;
            buf.retrieve_all();
            });
    client.start();
    // 主线程阻塞等待键盘输入
    char buf[1024];
    while (::fgets(buf, sizeof(buf), stdin)) {
        buf[strlen(buf) - 1] = '\0';
        if (client.is_connected()) {
            client.conn()->send(buf);
        } else {
            std::cout << "disconnect with server\n";
            break;
        }
    }
}
```
```cpp
int main()
{
    angel::evloop loop;
    angel::client client(&loop, angel::inet_addr("127.0.0.1:8000"));
    client.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            std::cout << buf.c_str() << "\n";
            buf.retrieve_all();
            });
    client.start();

    char buf[1024];
    angel::channel_ptr chl(new angel::channel(&loop));
    chl->set_fd(0); // 标准输入
    chl->set_read_handler([&buf, &client, &loop]{
            fgets(buf, sizeof(buf), stdin); // 每次读取一行
            if (client.is_connected()) {
                client.conn()->send(buf);
            } else {
                std::cout << "disconnect with server\n";
                loop.quit();
            }
            });
    loop.add_channel(chl);

    loop.run();
}
```
#### 将服务器多线程化
```cpp
int main(void)
{
    angel::evloop loop;
    angel::server server(&loop, angel::inet_addr(8888));
    server.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            conn->format_send("hello: %s", buf.c_str());
            buf.retrieve_all();
            });
    // 主线程接收新连接，4个io线程处理连接
    server.start_io_threads(4);
    server.start();
    loop.run();
}
```
#### 使用一个线程池处理任务
```cpp
int main(void)
{
    angel::evloop loop;
    angel::server server(&loop, angel::inet_addr(8888));
    server.set_message_handler([&server](const angel::connection_ptr& conn, angel::buffer& buf){
            // 这里必须拷贝一份conn，因为离开这个函数后conn就会被析构
            server.executor([conn, &buf]{
                    conn->format_send("hello: %s", buf.c_str());
                    buf.retrieve_all();
                    });
            });
    // 开启一个拥有4个线程的任务线程池
    server.start_task_threads(4);
    server.start();
    loop.run();
}
```
#### 踢掉空闲连接
```cpp
int main(void)
{
    int ttl = 3000;
    angel::evloop loop;
    angel::server server(&loop, angel::inet_addr(8888));
    server.set_connection_handler([ttl](const angel::connection_ptr& conn){
            conn->set_ttl(ttl);
            conn->format_send("hello, bye bye in %d s\n", ttl / 1000);
            });
    // ttl ms后连接关闭时该回调会被触发
    server.set_close_handler([](const angel::connection_ptr& conn){
            conn->send(">bye bye<\n");
            });
    server.start();
    loop.run();
}
```
#### 客户端连接超时处理
```cpp
int main(void)
{
    angel::evloop loop;
    angel::client client(&loop, angel::inet_addr("1.2.3.4:80"));
    client.set_connection_timeout_handler(6000, [&loop]{
            std::cout << "connection timeout\n";
            loop.quit();
            });
    client.set_connection_handler([](const angel::connection_ptr& conn){
            std::cout << "connection success\n";
            });
    client.start();
    loop.run();
}
```
#### 日志记录与控制
```cpp
int main(void)
{
    // 日志默认输出到文件中，存储于当前目录下的.log目录中
    // stdout表示将日志打印到标准输出
    angel::set_log_flush(angel::logger::flush_flags::stdout);
    // 只打印日志级别大于等于WARN的日志，默认打印INFO级别日志
    angel::set_log_level(angel::logger::level::warn);
    log_info("hello");
    log_warn("hello");
    log_error("hello");
}
```
### 更多示例
- 内置的[async dns resolver](https://github.com/yaomer/angel/tree/master/src/dns)
- 内置的[websocket server](https://github.com/yaomer/angel/tree/master/src/websocket)
- 内置的[httplib](https://github.com/yaomer/angel/tree/master/src/httplib)
- 内置的[smtp client](https://github.com/yaomer/angel/tree/master/src/smtplib)
- 一些简单的[例子](https://github.com/yaomer/angel/tree/master/sample)
- 一个更有意义的例子[alice](https://github.com/yaomer/alice)

### 性能测试
使用示例中简陋的[HTTP服务器](https://github.com/yaomer/angel/tree/master/examples/http)与Nginx作简单的性能对比测试

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
