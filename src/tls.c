#include "common.h"
#include "tls.h"
#include <openssl/err.h>
#include <unistd.h>

int ssl_connect(int sockfd, const char *hostname, SSL_CTX **ctx, SSL **ssl) {
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
