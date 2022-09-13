WebSocket Server
---
用法同`html5`提供的`websocket`接口类似

`WebSocketContext`表示一个连接的客户端上下文
+ `decoded_buffer`用于存放解码好的消息
+ `is_binary_type`表示接收到的消息是二进制还是文本
+ `origin`即为`http.handshake.origin`
+ `host`即为`http.handshake.host`

假设我们有一个`WebSocketServer`对象`ws`
+ `ws.onopen`在连接建立时触发
+ `ws.onmessage`在接收到客户端消息时触发
+ `ws.onclose`在连接关闭时触发
+ `ws.onerror`在发生错误时触发
+ `ws.for_each()`可用于遍历所有已连接的客户端

这5个参数都是`WebSocketHandler: std::function<void(WebSocketContext&)>`，
接收一个`WebSocketContext`参数供用户使用。

```cpp
#include <iostream>
#include <angel/ws-server.h>

int main()
{
    angel::evloop loop;
    angel::WebSocketServer ws(&loop, angel::inet_addr(8000));
    ws.onopen = [](angel::WebSocketContext& c){
        std::cout << "a new client (" << c.origin << ") coming...\n";
    };
    ws.onmessage = [&ws](angel::WebSocketContext& c){
        std::cout << "(" << c.origin << "): " << c.decoded_buffer << "\n";
        ws.for_each([&c](angel::WebSocketContext& oc){
                oc.send(c.decoded_buffer);
                });
    };
    ws.onclose = [](angel::WebSocketContext& c){
        std::cout << "disconnect with client (" << c.origin << ")\n";
    };
    ws.onerror = [](angel::WebSocketContext& c){
        std::cout << "client (" << c.origin << ") error: " << c.decoded_buffer << "\n";
    };
    ws.start();
    loop.run();
}
```

这个简单的`WS`服务监听本地端口`8000`，它将接收到的消息广播给所有连接的客户端

```
$ clang++ -std=c++17 -o server demo.cc -langel
$ ./server
```
借用`html5`的`websocket`接口可以很容易在浏览器控制台测试它
