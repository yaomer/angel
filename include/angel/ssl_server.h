#ifndef __ANGEL_SSL_SERVER_H
#define __ANGEL_SSL_SERVER_H

#include <angel/server.h>

namespace angel {

class ssl_handshake;

class ssl_server : public server {
public:
    ssl_server(evloop *, inet_addr);
    ~ssl_server();

    void set_cipher_list(const char *cipher_list);
    void set_certificate_file(const char *cert_file);
    void set_private_key_passwd(const char *key_passwd);
    void set_private_key_file(const char *key_file);
private:
    connection_ptr create_connection(channel *) override;
    void establish(channel *) override;
    std::unordered_map<int, std::unique_ptr<ssl_handshake>> shmap;
};

}

#endif // __ANGEL_SSL_SERVER_H
