#include "ssl_connection.h"

#include <sys/mman.h>

#include <angel/util.h>
#include <angel/logger.h>

namespace angel {

ssl_connection::ssl_connection(size_t id, class channel *chl, ssl_handshake *sh)
    : connection(id, chl), sh(sh),
    sf(std::make_unique<ssl_filter>(sh->get_ssl(), &decrypted, nullptr))
{
}

ssl_connection::~ssl_connection()
{
}

void ssl_connection::handle_message()
{
    sf->decrypt(&input_buf);
    if (decrypted.readable() > 0) {
        message_handler(shared_from_this(), decrypted);
    }
}

ssize_t ssl_connection::write(const char *data, size_t len)
{
    SSL *ssl = sh->get_ssl();
    int n = SSL_write(ssl, data, len);
    log_debug("SSL_write (%d) bytes to connection(id=%zu, fd=%d)", n, conn_id, channel->fd());
    if (n <= 0) {
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ) {
            log_warn("connection(id=%zu, fd=%d): SSL_ERROR_WANT_READ", conn_id, channel->fd());
            return -1;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            log_warn("connection(id=%zu, fd=%d): SSL_ERROR_WANT_WRITE", conn_id, channel->fd());
            return -1;
        } else {
            char buf[256];
            ERR_error_string(err, buf);
            log_error("connection(id=%zu, fd=%d): OpenSSL: %s", conn_id, channel->fd(), buf);
            force_close_connection();
            return -2;
        }
    }
    return n;
}

static const off_t ChunkSize = 1024 * 32;

// SSL_sendfile() is available only when ktls(Kernel TLS) is enabled.
// Here we fallback to mmap()/write().
ssize_t ssl_connection::sendfile(int fd, off_t offset, off_t count)
{
    off_t fix_off = util::page_aligned(offset);
    off_t diff  = offset - fix_off;
    off_t size  = std::min(ChunkSize, count) + diff;
    char *start = (char*)mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, fix_off);

    int n = write(start + diff, size - diff);
    munmap(start, size);
    return n;
}

}
