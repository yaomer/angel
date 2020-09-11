#include <angel/client.h>

#include <iostream>

char buf[1024 * 4];
int len = sizeof(buf);

int main()
{
    memset(buf, 's', len);
    angel::evloop loop;
    angel::client cli(&loop, angel::inet_addr("127.0.0.1:8000"));
    cli.set_connection_handler([](const angel::connection_ptr& conn){
            conn->send(buf, len);
            });
    cli.set_write_complete_handler([](const angel::connection_ptr& conn){
            conn->send(buf, len);
            });
    cli.start();
    loop.run();
}
