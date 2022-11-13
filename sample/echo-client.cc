#include <angel/evloop_thread.h>
#include <angel/client.h>

#include <iostream>

int main()
{
    angel::evloop_thread t_loop;
    angel::client cli(t_loop.get_loop(), angel::inet_addr("127.0.0.1:8000"));
    cli.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            std::cout << buf.c_str() << "\n";
            buf.retrieve_all();
            });
    cli.start();

    char buf[1024];
    while (::fgets(buf, sizeof(buf), stdin)) {
        buf[strlen(buf) - 1] = '\0';
        if (cli.is_connected()) {
            cli.conn()->send(buf);
        } else {
            std::cout << "disconnect with server\n";
            break;
        }
    }
}
