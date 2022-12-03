#ifndef __ANGEL_SSL_FILTER
#define __ANGEL_SSL_FILTER

#include <angel/buffer.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

namespace angel {

class ssl_filter {
public:
    // If `encrypted` is nullptr, the filter will only be used for decryption.
    // If `decrypted` is nullptr, the filter will only be used for encryption.
    ssl_filter(SSL *ssl, buffer *decrypted, buffer *encrypted);
    ~ssl_filter();

    ssl_filter(const ssl_filter&) = delete;
    ssl_filter& operator=(const ssl_filter&) = delete;

    // Call after read from the network.
    // The decrypted data is stored in buffer `decrypted`.
    void decrypt(buffer *input);
    // Call before write to the network.
    // The encrypted data is stored in buffer `encrypted`.
    void encrypt(buffer *output);
private:
    SSL *ssl;
    BIO *rbio;
    BIO *wbio;
    buffer *decrypted;
    buffer *encrypted;
    char buf[1024];
};

}

#endif // __ANGEL_SSL_FILTER
