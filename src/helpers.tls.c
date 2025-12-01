#include "helpers.tls.h"
#include <openssl/err.h>
#include <sys/socket.h>
#include <unistd.h>

int tls_connect(SSL **ssl, int sockfd,
                SSL_CTX **ctx, const char *hostname)
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    if (sockfd < 0 || !hostname || !ctx || !ssl) {
        return EINVAL;
    }

    *ctx = SSL_CTX_new(TLS_client_method());
    if (!*ctx) {
        ERR_print_errors_fp(stderr);
        return EIO;
    }

    *ssl = SSL_new(*ctx);
    if (!*ssl) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(*ctx);
        return EIO;
    }

    SSL_set_fd(*ssl, sockfd);
    SSL_set_tlsext_host_name(*ssl, hostname);
    int err = SSL_connect(*ssl);

    if (err <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(*ssl);
        SSL_CTX_free(*ctx);

        return ERR_get_error();
    }
    return 0;
}

void tls_free(SSL *ssl, SSL_CTX *ctx) {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}

ssize_t send_maybe_tls(SSL *ssl, int sockfd,
                       char *bytes, size_t len)
{
    if (sockfd < 0 && !ssl) {
        // nothing to write to!
        return -1;
    }

    if (ssl) {
        return SSL_write(ssl, bytes, len);
    } else {
        return send(sockfd, bytes, len, 0);
    }
}

ssize_t recv_maybe_tls(SSL *ssl, int sockfd,
                       void *buf, size_t len)
{
    if (sockfd < 0 && !ssl) {
        // nothing to write to!
        return -1;
    }

    if (ssl) {
        return SSL_read(ssl, buf, len);
    } else {
        return recv(sockfd, buf, len, 0);
    }
}
