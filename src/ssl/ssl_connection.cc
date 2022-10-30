#include "ssl_connection.h"

#include <angel/util.h>

namespace angel {

ssl_connection::ssl_connection(size_t id, evloop *loop, int sockfd, ssl_handshake *sh)
    : connection(id, loop, sockfd), sh(sh),
    sf(new ssl_filter(sh->get_ssl(), &decrypted, &encrypted))
{
}

ssl_connection::~ssl_connection()
{
}

void ssl_connection::set_message_handler(const message_handler_t handler)
{
    if (!handler) return;
    connection::set_message_handler([this, message_handler = std::move(handler)]
            (const connection_ptr& conn, buffer& buf){
            this->sf->decrypt(&buf);
            message_handler(conn, decrypted);
            });
}

void ssl_connection::send(const char *s, size_t len)
{
    output.append(s, len);
    sf->encrypt(&output);
    connection::send(encrypted.peek(), encrypted.readable());
    encrypted.retrieve_all();
}

void ssl_connection::send(std::string_view s)
{
    send(s.data(), s.size());
}

void ssl_connection::send(const void *v, size_t len)
{
    send(reinterpret_cast<const char*>(v), len);
}

void ssl_connection::format_send(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    auto res = util::format(fmt, ap);
    va_end(ap);
    send(res.buf, res.len);
    if (res.alloced) delete []res.buf;
}

}
