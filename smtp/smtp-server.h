#ifndef __SMTP_SERVER_H
#define __SMTP_SERVER_H

#include <angel/server.h>

struct smtp_context;

enum { INIT, EHLO, MAIL, RCPT, DATA, RCVDATA, RSET, QUIT };
enum { Ok, Error, NotEnough };

class smtp_server {
public:
    smtp_server(angel::evloop *, angel::inet_addr);
    ~smtp_server() = default;
    smtp_server(const smtp_server&) = delete;
    smtp_server& operator=(const smtp_server&) = delete;
private:
    void receive_mail(const angel::connection_ptr&, angel::buffer&);
    angel::server smtp;
    friend struct smtp_context;
};

struct smtp_context {
    int parse_ehlo(angel::buffer&);
    int parse_mail(angel::buffer&);
    int parse_rcpt(angel::buffer&);
    int parse_data(angel::buffer&);
    int recv_data(angel::buffer&);
    int parse_quit(angel::buffer&);
    void save_mail(const char *data, size_t len);
    int cmd = INIT;
    std::string origin;
    std::string mail_from;
    std::vector<std::string> mail_to;
};

#endif // __SMTP_SERVER_H
