#ifndef __ANGEL_SSL_HANDSHAKE_H
#define __ANGEL_SSL_HANDSHAKE_H

#include <angel/evloop.h>
#include <angel/channel.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace angel {

class ssl_handshake {
public:
    ssl_handshake(evloop *loop, SSL_CTX *ctx);
    ~ssl_handshake();

    ssl_handshake(const ssl_handshake&) = delete;
    ssl_handshake& operator=(const ssl_handshake&) = delete;

    // fd should be set nonblock,
    // and the underlying tcp connection should have been successfully established,
    // then try to establish a TLS/SSL connection on it.
    void start_client_handshake(int fd);
    void start_server_handshake(int fd);
    void shutdown();
    SSL *get_ssl() { return ssl; }

    std::function<void()> onestablish;
    std::function<void()> onfailed;
private:
    void start_handshake(int fd);
    void handshake(int fd);
    evloop *loop;
    std::shared_ptr<channel> channel;
    SSL_CTX *ctx;
    SSL *ssl;
};

}

#endif // __ANGEL_SSL_HANDSHAKE_H
