#include <angel/server.h>

// 这里我们用telnet充当client来测试fib-server
// 在本机上测试如下：
// $ telnet localhost 8000
// $ 42
// $ 267914296

// 递归方法显而易见是相当耗时的，这里我们用它只是为了用到thread pool，
// 此时直接在io线程计算显然是不合适的
static size_t fib(size_t n)
{
    if (n == 0 || n == 1)
        return n;
    return fib(n - 1) + fib(n - 2);
}

int main()
{
    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    serv.set_message_handler([&serv](const angel::connection_ptr& conn, angel::buffer& buf){
            size_t n = atol(buf.c_str());
            // 这里必须拷贝conn以避免失效
            serv.executor([n, conn]{
                    char s[64];
                    snprintf(s, sizeof(s), "%zu\n", fib(n));
                    conn->send(s);
                    });
            buf.retrieve_all();
            });
    // 用一个thread pool来计算fibonacci
    serv.start_task_threads();
    serv.start();
    loop.run();
}
