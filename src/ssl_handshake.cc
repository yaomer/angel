#include <angel/ssl_handshake.h>

#include <angel/logger.h>

namespace angel {

ssl_handshake::ssl_handshake(evloop *loop)
    : loop(loop), ctx(nullptr), ssl(nullptr)
{
}

ssl_handshake::~ssl_handshake()
{
    shutdown();
}

void ssl_handshake::start_client_handshake(int fd)
{
    SSL_library_init();
    ctx = SSL_CTX_new(SSLv23_client_method());
    ssl = SSL_new(ctx);
    SSL_set_connect_state(ssl);
    start_handshake(fd);
}

void ssl_handshake::start_server_handshake(int fd)
{
    SSL_library_init();
    ctx = SSL_CTX_new(SSLv23_server_method());
    ssl = SSL_new(ctx);
    SSL_set_accept_state(ssl);
    start_handshake(fd);
}

void ssl_handshake::start_handshake(int fd)
{
    SSL_set_fd(ssl, fd);
    log_info("(fd=%d) Start SSL handshake", fd);
    channel.reset(new class channel(loop));
    channel->set_fd(fd);
    auto check_handler = [this, fd]{ this->handshake(fd); };
    channel->set_read_handler(check_handler);
    channel->set_write_handler(check_handler);
    loop->add_channel(channel);
    handshake(fd);
}

void ssl_handshake::handshake(int fd)
{
    int rc = SSL_do_handshake(ssl);
    // SSL handshake was successfully completed.
    if (rc == 1) {
        log_info("(fd=%d) SSL handshake successful", fd);
        loop->remove_channel(channel);
        if (onestablish) onestablish(ssl);
        else shutdown();
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
    // When fd is readable or writable, just call handshake(fd) again,
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
        log_error("(fd=%d) SSL handshake failed: %s", fd, buf);
        loop->remove_channel(channel);
        if (onfailed) onfailed();
        else shutdown();
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
        log_info("Close SSL connection");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = nullptr;
    }
}

}
