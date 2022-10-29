#ifndef __ANGEL_SSL_CLIENT_H
#define __ANGEL_SSL_CLIENT_H

#include <angel/client.h>

#include <angel/ssl_handshake.h>
#include <angel/ssl_filter.h>

namespace angel {

class ssl_client : public client {
public:
    ssl_client(evloop *, inet_addr, client_options ops = client_options());
    ~ssl_client();
    void start() override;
    void restart() override;
    void restart(inet_addr peer_addr) override;
    void send(std::string_view data) override;
private:
    void new_connection(int fd);
    void establish(SSL *ssl, int fd);
    void close_connection();
    std::shared_ptr<ssl_handshake> sh;
    std::unique_ptr<ssl_filter> sf;
    buffer output;
    buffer decrypted;
    buffer encrypted;
};

}

#endif // __ANGEL_SSL_CLIENT_H
