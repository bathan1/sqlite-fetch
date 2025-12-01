#include <asm-generic/errno-base.h>
#include <netdb.h>
#include <stdio.h>

#include "helpers.tcp.h"

int tcp_getaddrinfo(const char *hostname, const char *port,
                    struct addrinfo **addr)
{
    if (!hostname || !port || !addr) {
        return EINVAL;
    }
    struct addrinfo hints = {
        .ai_family=AF_INET,
        .ai_socktype=SOCK_STREAM
    };
    int rc = getaddrinfo(hostname, port, &hints, addr);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s, %s, HINTS, ADDR): %s\n", hostname, port, gai_strerror(rc));
        return rc;
    }
    return 0;
}

int tcp_socket(struct addrinfo *addrinfo) {
    int sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    return sockfd;
}
