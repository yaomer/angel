#include "ssl_filter.h"

#include <angel/logger.h>

namespace angel {

ssl_filter::ssl_filter(SSL *ssl, buffer *decrypted, buffer *encrypted)
    : ssl(ssl), decrypted(decrypted), encrypted(encrypted)
{
    rbio = wbio = nullptr;
    if (decrypted) {
        rbio = BIO_new(BIO_s_mem());
        SSL_set0_rbio(ssl, rbio);
    }
    if (encrypted) {
        wbio = BIO_new(BIO_s_mem());
        SSL_set0_wbio(ssl, wbio);
    }
}

ssl_filter::~ssl_filter()
{
    // Nothing is to be done.
    // Invoking SSL_free() will indirectly free the affected BIO object.
}

void ssl_filter::decrypt(buffer *input)
{
    // Give the encrypted data read from the network to openssl for decryption.
    if (input->readable() > 0) {
        // Writes to memory BIOs will always succeed if memory is available:
        // that is their size can grow indefinitely.
        int n = BIO_write(rbio, input->peek(), input->readable());
        if (n > 0) input->retrieve(n);
    }

    // Read the decrypted data from rbio.
    while (true) {
        int n = SSL_read(ssl, buf, sizeof(buf));
        if (n > 0) {
            // The read operation was successful.
            decrypted->append(buf, n);
        } else {
            // As at any time a re-negotiation is possible,
            // a call to SSL_read() can also cause write operations!
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ) {
                // Nothing is to be done.
                // The next readable event trigger will satisfy the needs of SSL_read().
            } else if (err == SSL_ERROR_WANT_WRITE) {
                log_error("SSL decrypt: SSL_ERROR_WANT_WRITE");
            }
            break;
        }
    }
}

void ssl_filter::encrypt(buffer *output)
{
    // Give the unencrypted data write to the network to openssl for encryption.
    if (output->readable() > 0) {
        int n = SSL_write(ssl, output->peek(), output->readable());
        if (n > 0) {
            output->retrieve(n);
        } else {
            // As at any time a re-negotiation is possible,
            // a call to SSL_write() can also cause read operations!
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ) {
                log_error("SSL encrypt: SSL_ERROR_WANT_READ");
            } else if (err == SSL_ERROR_WANT_WRITE) {
                log_error("SSL encrypt: SSL_ERROR_WANT_WRITE");
            }
        }
    }

    // Read the encrypted data from wbio.
    while (true) {
        int n = BIO_read(wbio, buf, sizeof(buf));
        if (n > 0) encrypted->append(buf, n);
    }
}

}
