#ifndef __ANGEL_SSL_CLIENT_H
#define __ANGEL_SSL_CLIENT_H

#include <angel/client.h>

#include <angel/ssl_handshake.h>

namespace angel {

class ssl_client : public client {
public:
    ssl_client(evloop *, inet_addr, client_options ops = client_options());
    ~ssl_client();
private:
    static SSL_CTX *get_ssl_ctx();
    connection_ptr create_connection(int fd) override;
    void new_connection(int fd) override;
    std::unique_ptr<ssl_handshake> sh;
};

}

#endif // __ANGEL_SSL_CLIENT_H
