/**
 * @file tls.h
 * @brief openssl wrapper
 */

#include <openssl/ssl.h>

/**
 * @brief Perform a TLS handshake with peer SOCKFD and get back a status code.
 *
 * After connecting to HOSTNAME over tcp at SOCKFD, call #ssl_connect()
 * to initialize CTX and SSL.
 *
 * #ssl_connect() will *never* close SOCKFD. The **caller** is responsible
 * for doing so on any errors.
 *
 * @retval 0 OK - CTX and SSL allocated successfully.
 * @retval 22 EINVAL - At least one argument was `NULL` / invalid.
 * @retval 5 EIO - One of \c SSL_CTX_new() or \c SSL_new() failed. 
 *                Writes openssl error message to stderr.
 * @retval ~0 OTHER - \c SSL_connect() failed.
 */
int ssl_connect(int sockfd, const char *hostname, SSL_CTX **ctx, SSL **ssl);
