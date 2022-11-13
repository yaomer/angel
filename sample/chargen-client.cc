#include <angel/client.h>

#include <iostream>

int main()
{
    angel::evloop loop;
    angel::client cli(&loop, angel::inet_addr("127.0.0.1:8000"));
    cli.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            std::cout << buf.c_str();
            buf.retrieve_all();
            });
    cli.start();
    loop.run();
}
