#ifndef __ANGEL_SSL_SERVER_H
#define __ANGEL_SSL_SERVER_H

#include <angel/server.h>

#include <angel/ssl_handshake.h>
#include <angel/ssl_filter.h>

namespace angel {

class ssl_server : public server {
public:
    explicit ssl_server(evloop *, inet_addr);
    ~ssl_server();

    void start() override;
    void set_certificate(const char *path);
    void set_certificate_key(const char *path);
    void send(const connection_ptr& conn, std::string_view data);
private:
    struct ssl {
        std::unique_ptr<ssl_handshake> sh;
        std::unique_ptr<ssl_filter> sf;
        buffer output;
        buffer decrypted;
        buffer encrypted;
    };
    void establish(ssl *ssl, int fd);
    void new_connection(int fd);
    void remove_connection(const connection_ptr& conn);
    static SSL_CTX *get_ssl_ctx();
    std::unordered_map<size_t, std::shared_ptr<ssl>> ssl_map;
};

}

#endif // __ANGEL_SSL_SERVER_H
