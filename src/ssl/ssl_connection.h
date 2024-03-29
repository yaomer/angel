#ifndef __ANGEL_SSL_CONNECTION_H
#define __ANGEL_SSL_CONNECTION_H

#include <angel/connection.h>

#include "ssl_handshake.h"
#include "ssl_filter.h"

namespace angel {

class ssl_connection : public connection {
public:
    ssl_connection(size_t id, class channel *, ssl_handshake *);
    ~ssl_connection();
private:
    void handle_message() override;
    ssize_t write(const char *data, size_t len) override;
    ssize_t sendfile(int fd, off_t offset, off_t count) override;

    std::shared_ptr<ssl_handshake> sh;
    std::unique_ptr<ssl_filter> sf;
    buffer decrypted;
};

}

#endif // __ANGEL_SSL_CONNECTION_H
