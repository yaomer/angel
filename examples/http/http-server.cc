#include <angel/server.h>

#include <iostream>

#include "http-context.h"

void message_handler(const angel::connection_ptr& conn, angel::buffer& buf)
{
    if (!conn->is_connected()) return;
    log_info(buf.c_str());
    auto& context = std::any_cast<http_context&>(conn->get_context());
    while (buf.readable() >= 2) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        switch (context.get_state()) {
        case http_context::PARSE_LINE:
            context.parse_line(buf.peek(), buf.peek() + crlf + 2);
            break;
        case http_context::PARSE_HEADER:
            context.parse_header(buf.peek(), buf.peek() + crlf + 2);
            if (context.get_state() == http_context::PARSE_LINE)
                context.send_response(conn);
            break;
        }
        buf.retrieve(crlf + 2);
    }
}

int main()
{
    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    serv.set_connection_handler([](const angel::connection_ptr& conn){
            conn->set_context(http_context());
            });
    serv.set_message_handler(message_handler);
    std::cout << "Server is running at localhost:8000\n";
    serv.start();
    loop.run();
}
