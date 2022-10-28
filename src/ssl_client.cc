#include <angel/ssl_client.h>

#include "connector.h"

namespace angel {

ssl_client::ssl_client(evloop *loop, inet_addr peer_addr, client_options ops)
    : client(loop, peer_addr, ops)
{
}

ssl_client::~ssl_client()
{
}

void ssl_client::start()
{
    add_connection_timeout_timer();
    connector.reset(new connector_t(loop, peer_addr));
    connector->retry_interval = ops.retry_interval_ms;
    connector->protocol = ops.protocol;

    // We hook new_connection_handler, message_handler and close_handler
    connector->new_connection_handler = [this](int fd) {
        // At this time, the TCP connection has been established,
        // we try to complete SSL handshake.
        sh.reset(new ssl_handshake(loop));
        sh->start_client_handshake(fd);
        sh->onestablish = [this, fd](SSL *ssl){
            sf.reset(new ssl_filter(ssl, &decrypted, &encrypted));
            new_connection(fd);
        };
        sh->onfailed = [this]{ close_connection(cli_conn); };
    };

    if (message_handler) {
        message_handler = [this, handler = std::move(message_handler)]
            (const connection_ptr& conn, buffer& buf){
            sf->decrypt(&buf);
            handler(conn, decrypted);
            };
    }

    // Close the SSL connection before closing the underlying TCP connection.
    close_handler = [this, handler = std::move(close_handler)](const connection_ptr& conn) {
        if (handler) handler(conn);
        sh->shutdown();
    };

    connector->connect();
}

void ssl_client::send(std::string_view data)
{
    output.append(data);
    sf->encrypt(&output);
    cli_conn->send(encrypted.peek(), encrypted.readable());
    encrypted.retrieve_all();
}

}
