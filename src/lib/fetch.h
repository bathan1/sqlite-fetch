/**
 * @file fetch.h
 * @brief Simplified [Web Fetch](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API) implementation for C.
 */

#pragma once
#include "prefix.h"
#include <openssl/types.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * Taken from Web API URL, word 4 word, bar 4 bar.
 */
struct url {
    /**
     * @brief A string containing the domain (that is the hostname) followed by
     * (if a port was specified) a ':' and the port of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/host}
     */
    prefixed *host;

    /**
     * @brief A string containing the domain of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/hostname}
     */
    prefixed *hostname;

    /**
     * @brief A string containing an initial '/' followed by the path of the URL,
     * not including the query string or fragment.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/pathname}
     */
    prefixed *pathname;

    /**
     * @brief A string containing the port number of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/port}
     */
    prefixed *port;

    /**
     * @brief A string containing the protocol scheme of the URL, including the final ':'.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/protocol}
     */
    prefixed *protocol;
};
void url_free(struct url *url);

struct dispatch {
    int sockfd;
    SSL *ssl;
    SSL_CTX *ctx;
    struct url url;
    struct addrinfo *addrinfo;
};
void dispatch_free(struct dispatch *dispatch);
struct dispatch *fetch_socket(const char *url, const char *init[4]);
int use_fetch(int fds[4], struct dispatch *dispatch);

struct fetch_state {
    /* FDs */
    int netfd;        // TCP socket (nonblocking)
    int outfd;        // socketpair writer FD (nonblocking)
    int ep;           // epoll instance FD

    char *hostname;
    SSL_CTX *ssl_ctx;
    SSL     *ssl;

    /* --- HTTP HEADER PARSING --- */
    bool headers_done;
    char header_buf[8192];  // store header bytes
    size_t header_len;

    bool chunked_mode;
    size_t content_length;

    /* --- CHUNKED DECODING STATE --- */
    bool reading_chunk_size;    // true = reading hex size line
    char chunk_line[128];       // buffer for chunk-size line
    size_t chunk_line_len;      // how many chars collected
    size_t current_chunk_size;  // remaining bytes in current chunk
    int expecting_crlf;         // 2 -> expecting "\r\n"

    FILE *bass[2];

    /* --- NONBLOCKING SEND STATE FOR outfd --- */
    char *pending_buf;         // partial write buffer (JSON object)
    size_t pending_len;         // bytes left to send
    size_t pending_off;         // current offset in pending_send

    /* --- TERMINATION STATE --- */
    bool http_done;             // reached end of chunked stream or TCP closed
    bool closed_outfd;          // have we closed outfd yet?
};

void *fetcher(void *arg);
