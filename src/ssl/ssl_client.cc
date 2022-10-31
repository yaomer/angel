#include <angel/ssl_client.h>

#include "ssl_connection.h"

namespace angel {

ssl_client::ssl_client(evloop *loop, inet_addr peer_addr, client_options ops)
    : client(loop, peer_addr, ops)
{
}

ssl_client::~ssl_client()
{
}

namespace {
    struct ssl_ctx_free {
        void operator()(SSL_CTX *ctx) {
            SSL_CTX_free(ctx);
        }
    };
}

// Just create a SSL_CTX for all clients of each thread.
SSL_CTX *ssl_client::get_ssl_ctx()
{
    static thread_local std::unique_ptr<SSL_CTX, ssl_ctx_free> ctx;
    if (!ctx) {
        OPENSSL_init();
        ctx.reset(SSL_CTX_new(TLS_client_method()));
    }
    return ctx.get();
}

connection_ptr ssl_client::create_connection(int fd)
{
    return std::make_shared<ssl_connection>(1, loop, fd, sh.release());
}

// At this time, the TCP connection has been established,
// we try to complete SSL handshake.
void ssl_client::new_connection(int fd)
{
    sh.reset(new ssl_handshake(loop, get_ssl_ctx()));
    sh->start_client_handshake(fd);
    sh->onestablish = [this, fd]{ client::new_connection(fd); };
    sh->onfailed = [sh = sh.get(), fd]{ sh->shutdown(); close(fd); };
}

}
