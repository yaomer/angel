#include <angel/ssl_client.h>

#include <angel/logger.h>

#include "connector.h"

namespace angel {

ssl_client::ssl_client(evloop *loop, inet_addr peer_addr, client_options ops)
    : client(loop, peer_addr, ops)
{
}

ssl_client::~ssl_client()
{
    close_connection();
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
        SSL_library_init();
        ctx.reset(SSL_CTX_new(SSLv23_client_method()));
    }
    return ctx.get();
}

// At this time, the TCP connection has been established,
// we try to complete SSL handshake.
void ssl_client::new_connection(int fd)
{
    sh.reset(new ssl_handshake(loop, get_ssl_ctx()));
    sh->start_client_handshake(fd);
    sh->onestablish = [this, fd]{ this->establish(fd); };
    sh->onfailed = [this]{ close_connection(); };
}

void ssl_client::establish(int fd)
{
    log_info("SSL client(fd=%d) connected to host (%s)", fd, peer_addr.to_host());

    sf.reset(new ssl_filter(sh->get_ssl(), &decrypted, &encrypted));

    cli_conn = connection_ptr(new connection(1, loop, fd));
    cli_conn->set_connection_handler(connection_handler);
    cli_conn->set_high_water_mark_handler(high_water_mark, high_water_mark_handler);

    if (message_handler) {
        cli_conn->set_message_handler([this](const connection_ptr& conn, buffer& buf){
                this->sf->decrypt(&buf);
                message_handler(conn, decrypted);
                });
    }
    cli_conn->set_close_handler([this](const connection_ptr& conn){
            this->close_connection();
            if (this->ops.is_reconnect) this->start();
            });
    client::establish();
}

void ssl_client::close_connection()
{
    if (!cli_conn) return;
    if (cli_conn->is_closed()) return;
    if (close_handler) close_handler(cli_conn);
    cli_conn->set_state(connection::state::closed);
    loop->remove_channel(cli_conn->channel);
    // Close SSL connection and underlying TCP connection after remove_channel()
    loop->run_in_loop([sh = sh, conn = cli_conn](){ sh->shutdown(); });
    cancel_connection_timeout_timer();
    if (ops.is_quit_loop) loop->quit();
}

void ssl_client::start()
{
    add_connection_timeout_timer();
    connector.reset(new connector_t(loop, peer_addr));
    connector->retry_interval = ops.retry_interval_ms;
    connector->protocol = ops.protocol;
    connector->new_connection_handler = [this](int fd) {
        this->new_connection(fd);
    };
    connector->connect();
}

void ssl_client::restart()
{
    close_connection();
    start();
}

void ssl_client::restart(inet_addr peer_addr)
{
    this->peer_addr = peer_addr;
    restart();
}

void ssl_client::send(std::string_view data)
{
    output.append(data);
    sf->encrypt(&output);
    cli_conn->send(encrypted.peek(), encrypted.readable());
    encrypted.retrieve_all();
}

}
