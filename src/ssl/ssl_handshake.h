#ifndef __ANGEL_SSL_HANDSHAKE_H
#define __ANGEL_SSL_HANDSHAKE_H

#include <angel/channel.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace angel {

// The underlying tcp connection should have been successfully established,
// then try to establish a TLS/SSL connection on it.
class ssl_handshake {
public:
    ssl_handshake(class channel *, SSL_CTX *);
    ~ssl_handshake();

    ssl_handshake(const ssl_handshake&) = delete;
    ssl_handshake& operator=(const ssl_handshake&) = delete;

    void start_client_handshake();
    void start_server_handshake();
    SSL *get_ssl() const { return ssl; }

    // Called after SSL handshake is completed successfully.
    std::function<void(channel*)> onestablish;
    std::function<void()> onfail;
private:
    void start_handshake();
    void handshake();
    void shutdown();

    channel *channel;
    SSL_CTX *ctx;
    SSL *ssl;
};

}

#endif // __ANGEL_SSL_HANDSHAKE_H
