#include <angel/ssl_client.h>

#include "ssl_handshake.h"
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
static SSL_CTX *get_ssl_ctx()
{
    static thread_local std::unique_ptr<SSL_CTX, ssl_ctx_free> ctx;
    if (!ctx) {
        OPENSSL_init();
        ctx.reset(SSL_CTX_new(TLS_client_method()));
    }
    return ctx.get();
}

connection_ptr ssl_client::create_connection(channel *chl)
{
    return std::make_shared<ssl_connection>(get_next_id(), chl, sh.release());
}

// At this time, the TCP connection has been established,
// we try to complete SSL handshake.
void ssl_client::establish(channel *chl)
{
    sh = std::make_unique<ssl_handshake>(chl, get_ssl_ctx());
    sh->onestablish = [this](channel *chl){ client::establish(chl); };
    sh->onfail      = [this]{ if (connection_failure_handler) connection_failure_handler(); };
    sh->start_client_handshake();
}

}
