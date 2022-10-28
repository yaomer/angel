#ifndef __ANGEL_SSL_FILTER
#define __ANGEL_SSL_FILTER

#include <angel/buffer.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

namespace angel {

class ssl_filter {
public:
    ssl_filter(SSL *ssl, buffer *decrypted, buffer *encrypted);
    ~ssl_filter();
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
