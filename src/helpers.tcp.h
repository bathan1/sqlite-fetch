#include <netdb.h>

/**
 * @brief Write address info from HOSTNAME and PORT into ADDR.
 */
int tcp_getaddrinfo(const char *hostname, const char *port, struct addrinfo **addr);

/**
 * @brief Get socket fd from ADDRINFO.
 */
int tcp_socket(struct addrinfo *addrinfo);
