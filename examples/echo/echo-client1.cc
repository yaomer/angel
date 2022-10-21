#include <angel/client.h>

#include <iostream>

int main()
{
    angel::evloop loop;
    angel::client cli(&loop, angel::inet_addr("127.0.0.1:8000"));
    cli.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            std::cout << buf.c_str() << "\n";
            buf.retrieve_all();
            });
    cli.start();

    char buf[1024];
    angel::channel_ptr chl(new angel::channel(&loop));
    chl->set_fd(0);
    chl->set_read_handler([&buf, &cli, &loop]{
            fgets(buf, sizeof(buf), stdin);
            if (cli.is_connected()) {
                cli.conn()->send(buf);
            } else {
                std::cout << "disconnect with server\n";
                loop.quit();
            }
            });
    loop.add_channel(chl);

    loop.run();
}
