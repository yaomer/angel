#include <angel/ssl_server.h>

#include <angel/logger.h>

#include "ssl_connection.h"

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

    static std::string cipher_list;
    static std::string cert_file;
    static std::string key_passwd;
    static std::string key_file;
}

static SSL_CTX *get_ssl_ctx()
{
    int rc;
    static thread_local std::unique_ptr<SSL_CTX, ssl_ctx_free> ctx;
    if (!ctx) {
        OPENSSL_init();
        ctx.reset(SSL_CTX_new(TLS_server_method()));

        if (!cipher_list.empty()) {
            rc = SSL_CTX_set_cipher_list(ctx.get(), cipher_list.c_str());
            if (rc != 1) {
                log_fatal("SSL_CTX_set_cipher_list(%s) failed", cipher_list.c_str());
            }
        }

        rc = SSL_CTX_use_certificate_file(ctx.get(), cert_file.c_str(), SSL_FILETYPE_PEM);
        if (rc != 1) {
            log_fatal("SSL_CTX_use_certificate_file(%s) failed", cert_file.c_str());
        }

        SSL_CTX_set_default_passwd_cb_userdata(ctx.get(), (void*)key_passwd.c_str());

        rc = SSL_CTX_use_PrivateKey_file(ctx.get(), key_file.c_str(), SSL_FILETYPE_PEM);
        if (rc != 1) {
            log_fatal("SSL_CTX_use_PrivateKey_file(%s) failed", key_file.c_str());
        }

        rc = SSL_CTX_check_private_key(ctx.get());
        if (rc != 1) {
            log_fatal("SSL_CTX_check_private_key failed");
        }
    }
    return ctx.get();
}

connection_ptr ssl_server::create_connection(int fd)
{
    size_t id = conn_id++;
    evloop *io_loop = get_next_loop();
    auto it = shmap.find(fd);
    assert(it != shmap.end());
    auto *sh = it->second.release();
    shmap.erase(it);
    return std::make_shared<ssl_connection>(id, io_loop, fd, sh);
}

void ssl_server::new_connection(int fd)
{
    auto *sh = new ssl_handshake(loop, get_ssl_ctx());
    shmap.emplace(fd, sh);
    sh->start_server_handshake(fd);
    sh->onestablish = [this, fd]{ server::new_connection(fd); };
    sh->onfailed = [this, fd]{ shmap.erase(fd); close(fd); };
}

void ssl_server::set_cipher_list(const char *your_cipher_list)
{
    cipher_list = your_cipher_list;
}

void ssl_server::set_certificate_file(const char *your_cert_file)
{
    cert_file = your_cert_file;
}

void ssl_server::set_private_key_passwd(const char *your_key_passwd)
{
    key_passwd = your_key_passwd;
}

void ssl_server::set_private_key_file(const char *your_key_file)
{
    key_file = your_key_file;
}

}
