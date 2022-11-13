#include <angel/client.h>

#include <iostream>
#include <time.h>

int main()
{
    angel::evloop loop;
    angel::client cli(&loop, angel::inet_addr("127.0.0.1:8000"));
    cli.set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            if (buf.readable() >= sizeof(int32_t)) {
                char time[32];
                int32_t tm32 = *reinterpret_cast<int32_t*>(buf.peek());
                time_t tm = static_cast<time_t>(tm32);
                std::cout << ctime_r(&tm, time);
                buf.retrieve(sizeof(int32_t));
            } else {
                std::cout << "not enough data\n";
            }
            });
    cli.start();
    loop.run();
}
