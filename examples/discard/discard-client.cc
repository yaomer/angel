#include <angel/client.h>
#include <angel/util.h>

#include <fcntl.h>

#include <iostream>

char buf[1024 * 4];
int len = sizeof(buf);

void send_byte(const angel::connection_ptr& conn)
{
    conn->send(buf, len);
    conn->set_send_complete_handler([](const angel::connection_ptr& conn){
            send_byte(conn);
            });
}

void send_file(const angel::connection_ptr& conn)
{
    int fd = open("./server", O_RDONLY);
    conn->send_file(fd, 0, angel::util::get_file_size(fd));
    conn->set_send_complete_handler([fd](const angel::connection_ptr& conn){
            close(fd);
            send_file(conn);
            });
}

int main()
{
    memset(buf, 's', len);
    angel::evloop loop;
    angel::client cli(&loop, angel::inet_addr("127.0.0.1:8000"));
    cli.set_connection_handler([](const angel::connection_ptr& conn){
            send_file(conn);
            });
    cli.start();
    loop.run();
}
