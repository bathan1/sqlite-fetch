/**
 * @file tls.h
 * @brief openssl wrapper
 */

#include <openssl/ssl.h>
#include <stdbool.h>

/**
 * @brief Perform a TLS handshake with peer SOCKFD and get back a status code.
 *
 * After connecting to HOSTNAME over tcp at SOCKFD, call #connect_ssl()
 * to initialize CTX and SSL.
 *
 * #connect_ssl() will *never* close SOCKFD. The **caller** is responsible
 * for doing so on any errors.
 *
 * @retval 0 OK - CTX and SSL allocated successfully.
 * @retval 22 EINVAL - At least one argument was `NULL` / invalid.
 * @retval 5 EIO - One of \c SSL_CTX_new() or \c SSL_new() failed. 
 *                Writes openssl error message to stderr.
 * @retval ~0 OTHER - \c SSL_connect() failed.
 */
int connect_ssl(SSL **ssl, int sockfd, SSL_CTX **ctx, const char *hostname);

/** 
 * @brief Shutdown and free SSL and CTX.
 *
 * If both SSL and CTX are NULL, then this is effectively a no-op.
 */
void cleanup_ssl(SSL *ssl, SSL_CTX *ctx);

/**
 * @brief Maybe send LEN BYTES over tcp connection at SSL, if it exists.
 *
 * If SSL is `NULL`, then #send_maybe_tls() fallsback to plain \c send() call to SOCKFD.
 *
 * @retval -1 ERROR - Neither the SOCKFD or SSL sockets can be written to.
 * @retval (x >= 0) OK - X bytes written out.
 */
ssize_t send_maybe_tls(SSL *ssl, int sockfd, char *bytes, size_t len);

/**
 * @brief Maybe recv LEN BYTES over the TCP connection at SSL, if it exists.
 *
 * If SSL is `NULL` then #recv_maybe_tls() fallsback to plain \c recv() call to SOCKFD.
 *
 * @retval -1 ERROR - Neither the SOCKFD or SSL sockets can be written to.
 * @retval (x >= 0) OK - X bytes written out.
 */
ssize_t recv_maybe_tls(SSL *ssl, int sockfd, void *buf, size_t len);
