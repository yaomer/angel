#ifndef __ANGEL_SSL_CLIENT_H
#define __ANGEL_SSL_CLIENT_H

#include <angel/client.h>

namespace angel {

class ssl_handshake;

class ssl_client : public client {
public:
    ssl_client(evloop *, inet_addr, client_options ops = client_options());
    ~ssl_client();
private:
    connection_ptr create_connection(channel *) override;
    void establish(channel *) override;
    std::unique_ptr<ssl_handshake> sh;
};

}

#endif // __ANGEL_SSL_CLIENT_H
