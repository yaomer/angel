#include <angel/ssl_server.h>

#include "listener.h"

namespace angel {

ssl_server::ssl_server(evloop *loop, inet_addr listen_addr)
    : server(loop, listen_addr)
{
}

ssl_server::~ssl_server()
{
}

namespace {
    struct ssl_ctx_free {
        void operator()(SSL_CTX *ctx) {
            SSL_CTX_free(ctx);
        }
    };
}

static std::string certificate;
static std::string certificate_key;

SSL_CTX *ssl_server::get_ssl_ctx()
{
    static thread_local std::unique_ptr<SSL_CTX, ssl_ctx_free> ctx;
    if (!ctx) {
        SSL_library_init();
        ctx.reset(SSL_CTX_new(SSLv23_server_method()));
        int rc = SSL_CTX_use_certificate_chain_file(ctx.get(), certificate.c_str());
        assert(rc == 1);
        rc = SSL_CTX_use_PrivateKey_file(ctx.get(), certificate_key.c_str(), SSL_FILETYPE_PEM);
        assert(rc == 1);
    }
    return ctx.get();
}

void ssl_server::new_connection(int fd)
{
    ssl *ssl = new struct ssl();

    ssl->sh.reset(new ssl_handshake(loop, get_ssl_ctx()));
    ssl->sh->start_server_handshake(fd);
    ssl->sh->onestablish = [this, ssl, fd]{ this->establish(ssl, fd); };
    ssl->sh->onfailed = [ssl, fd]{ delete ssl; close(fd); };
}

void ssl_server::establish(ssl *ssl, int fd)
{
    size_t id = conn_id++;

    ssl->sf.reset(new ssl_filter(ssl->sh->get_ssl(), &ssl->decrypted, &ssl->encrypted));
    ssl_map.emplace(id, ssl);

    evloop *io_loop = server::get_next_loop();
    connection_ptr conn(new connection(id, io_loop, fd));
    conn->set_connection_handler(connection_handler);
    conn->set_high_water_mark_handler(high_water_mark, high_water_mark_handler);

    if (message_handler) {
        conn->set_message_handler([this](const connection_ptr& conn, buffer& buf){
                auto& ssl = ssl_map[conn->id()];
                ssl->sf->decrypt(&buf);
                message_handler(conn, ssl->decrypted);
                });
    }
    conn->set_close_handler([this](const connection_ptr& conn){
            this->remove_connection(conn);
            });
    server::establish(conn);
}

void ssl_server::remove_connection(const connection_ptr& conn)
{
    if (close_handler) close_handler(conn);
    conn->set_state(connection::state::closed);
    conn->get_loop()->remove_channel(conn->channel);
    loop->run_in_loop([this, id = conn->id()]{
            this->ssl_map.erase(id);
            this->connection_map.erase(id);
            });
}

void ssl_server::set_certificate(const char *path)
{
    certificate = path;
}

void ssl_server::set_certificate_key(const char *path)
{
    certificate_key = path;
}

void ssl_server::start()
{
    server::handle_signals();
    log_info("SSL Server (%s) is running", listener->addr().to_host());
    listener->new_connection_handler = [this](int fd){ this->new_connection(fd); };
    listener->listen();
}

void ssl_server::send(const connection_ptr& conn, std::string_view data)
{
    auto& ssl = ssl_map[conn->id()];
    ssl->output.append(data);
    ssl->sf->encrypt(&ssl->output);
    conn->send(ssl->encrypted.peek(), ssl->encrypted.readable());
    ssl->encrypted.retrieve_all();
}

}
