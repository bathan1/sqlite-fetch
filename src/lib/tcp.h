/**
 * @file tcp.h
 * @brief Socket management
 */
#include <netdb.h>

/**
 * @brief Write address info from HOSTNAME and PORT into ADDR.
 *
 * @retval 0 OK. Address Info written to ADDR.
 * @retval 22 EINVAL. At least one of HOSTNAME or PORT or ADDR is NULL.
 * @retval ANYTHING_ELSE Error. The return value of \c getaddrinfo() when non-zero.
 */
int tcp_getaddrinfo(const char *hostname, const char *port, struct addrinfo **addr);

/**
 * @brief Open socket fd from ADDRINFO and return it.
 *
 * @retval -1 Error opening socket.
 * @retval NONNEGATIVE OK, a socket file descriptor.
 */
int tcp_socket(struct addrinfo *addrinfo);
