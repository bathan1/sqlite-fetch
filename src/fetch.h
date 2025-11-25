#pragma once
#include "parse.h"
#include "web.h"
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

/**
 * Connects to host at URL over tcp, then returns the socket fd
 * *after* sending the corresponding HTTP request encoded in URL.
 */
int fetch(const char *url) {
    struct url_s *URL = parse_url(url);
    if (!URL) {
        return neg1(errno);
    }
    struct buffer_s *host = URL->host;
    struct buffer_s *port = URL->port;
    struct buffer_s *pathname = URL->pathname;



    struct addrinfo hints = {
        .ai_family=AF_INET,
        .ai_socktype=SOCK_STREAM
    };
    struct addrinfo *res = 0;
    int rc = getaddrinfo(host->bytes, port->bytes, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        url_free(URL);
        return 0;
    }
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        url_free(URL);
        return 0;
    }
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        freeaddrinfo(res);
        close(sockfd);
        url_free(URL);
        return 1;
    }
    freeaddrinfo(res);

    char *request = mk_GET(URL);
    if (!request) {
        close(sockfd);
        url_free(URL);
        return neg1(errno);
    }
    ssize_t sent = send(sockfd, request, strlen(request), 0);
    if (sent < 0) {
        perror("send");
        close(sockfd);
        url_free(URL);
        return neg1(errno);
    }

    return sockfd;
}
