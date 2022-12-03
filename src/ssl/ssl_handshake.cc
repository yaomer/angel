#include "ssl_handshake.h"

#include <angel/evloop.h>
#include <angel/logger.h>

namespace angel {

ssl_handshake::ssl_handshake(class channel *chl, SSL_CTX *ctx)
    : channel(chl), ctx(ctx), ssl(nullptr)
{
}

ssl_handshake::~ssl_handshake()
{
    shutdown();
}

void ssl_handshake::start_client_handshake()
{
    ssl = SSL_new(ctx);
    SSL_set_connect_state(ssl);
    channel->get_loop()->run_in_loop([this]{ start_handshake(); });
}

void ssl_handshake::start_server_handshake()
{
    ssl = SSL_new(ctx);
    SSL_set_accept_state(ssl);
    channel->get_loop()->run_in_loop([this]{ start_handshake(); });
}

void ssl_handshake::start_handshake()
{
    SSL_set_fd(ssl, channel->fd());
    log_info("(fd=%d) Start SSL handshake", channel->fd());
    auto check_handler = [this]{ this->handshake(); };
    channel->set_read_handler(check_handler);
    channel->set_write_handler(check_handler);
    handshake();
}

void ssl_handshake::handshake()
{
    int rc = SSL_do_handshake(ssl);
    // SSL handshake was successfully completed.
    if (rc == 1) {
        log_info("(fd=%d) SSL handshake successful", channel->fd());
        if (onestablish) {
            // Transfer the ownership of the channel to the upper layer.
            onestablish(channel);
            channel = nullptr;
        } else {
            shutdown();
        }
        return;
    }
    // SSL handshake in progress.
    //
    // In this case a call to SSL_get_error() with the return value of SSL_do_handshake()
    // will yield SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE.
    //
    // The calling process then must repeat the call after taking appropriate action to
    // satisfy the needs of SSL_do_handshake().
    //
    // When using a non-blocking socket, nothing is to be done, but select()
    // can be used to check for the required condition.
    //
    // When fd is readable or writable, just call handshake() again,
    // until the handshake is complete.
    int err = SSL_get_error(ssl, rc);
    if (err == SSL_ERROR_WANT_READ) {
        channel->enable_read();
        channel->disable_write();
    } else if (err == SSL_ERROR_WANT_WRITE) {
        channel->enable_write();
        channel->disable_read();
    } else { // fatal error
        char buf[256];
        ERR_error_string(err, buf);
        log_error("(fd=%d) SSL handshake failed: %s", channel->fd(), buf);
        shutdown();
        // `this` may be deleted in onfail().
        if (onfail) onfail();
    }
}

// SSL_shutdown() tries to send the "close notify" shutdown alert to the peer.
// Whether the operation succeeds or not, the SSL_SENT_SHUTDOWN flag is set and
// a currently open session is considered closed and good and will be kept in
// the session cache for further reuse.
//
// We don't have to call SSL_shutdown() again to receive the "close notify"
// shutdown alert from the peer, because we will then close the underlying
// TCP connection.
void ssl_handshake::shutdown()
{
    if (ssl) {
        log_info("(fd=%d) Close SSL connection", SSL_get_wfd(ssl));
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (channel) {
        channel->remove();
        channel = nullptr;
    }
}

}
