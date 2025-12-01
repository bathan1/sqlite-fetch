#include <asm-generic/errno-base.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers.tcp.h"
#include "helpers.fetch.h"

int tcp_getaddrinfo(const char *hostname, const char *port,
                    struct addrinfo **addr)
{
    if (!hostname || !port || !addr) {
        prefixed *errmsg = prefix(
            "tcp_getaddrinfo(\n",
            "    "
            "hostname=%s,\n"
            "    "
            "port=%s,\n"
            "    "
            "addr=%p\n"
            "): Can't pass in NULL.\n"
        );
        fprintf(stderr, "%s", str(errmsg));
        free(errmsg);
        return EINVAL;
    }
    struct addrinfo hints = {
        .ai_family=AF_INET,
        .ai_socktype=SOCK_STREAM
    };
    int rc = getaddrinfo(hostname, port, &hints, addr);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(hostname=%s, port=%s, HINTS, ADDR): %s\n", hostname, port, gai_strerror(rc));
        return rc;
    }
    return 0;
}

int tcp_socket(struct addrinfo *addrinfo) {
    int sockfd = socket(
        addrinfo->ai_family,
        addrinfo->ai_socktype,
        addrinfo->ai_protocol
    );
    if (sockfd < 0) {
        prefixed *errmsg = prefix(
            "Error opening socket(\n",
            "    "
            "DOMAIN=%p,\n"
            "    "
            "TYPE=%p,\n"
            "    "
            "PROTOCOL=%p\n"
            ")"
        );
        perror(str(errmsg));
        free(errmsg);
        return -1;
    }
    return sockfd;
}
