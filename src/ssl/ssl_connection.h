#ifndef __ANGEL_SSL_CONNECTION_H
#define __ANGEL_SSL_CONNECTION_H

#include <angel/connection.h>

#include <angel/ssl_handshake.h>
#include <angel/ssl_filter.h>

namespace angel {

class ssl_connection : public connection {
public:
    ssl_connection(size_t id, evloop *loop, int sockfd, ssl_handshake *sh);
    ~ssl_connection();

    void set_message_handler(const message_handler_t handler) override;
    void send(std::string_view s) override;
    void send(const char *s, size_t len) override;
    void send(const void *v, size_t len) override;
    void format_send(const char *fmt, ...) override;
private:
    std::shared_ptr<ssl_handshake> sh;
    std::unique_ptr<ssl_filter> sf;
    buffer output;
    buffer decrypted;
    buffer encrypted;
};

}

#endif // __ANGEL_SSL_CONNECTION_H
