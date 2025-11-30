#define _GNU_SOURCE
#include "common.h"
#include "bassoon.h"
#include "fetch.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <pthread.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/epoll.h>

struct fetch_state {
    /* FDs */
    int netfd;        // TCP socket (nonblocking)
    int outfd;        // socketpair writer FD (nonblocking)
    int ep;           // epoll instance FD
    
    char *hostname;
    bool is_tls;
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

/**
 * Taken from Web API URL, word 4 word, bar 4 bar.
 */
struct url {
    /**
     * A string containing the domain of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL}
     */
    str *hostname;

    /**
     * A string containing an initial '/' followed by the path of the URL, 
     * not including the query string or fragment.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/pathname}
     */
    str *pathname;

    /**
     * A string containing the port number of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/port}
     */
    str *port;
};

static char *host(struct url *url);

static int url_free(struct url *url);
static int tcp_getaddrinfo(struct url *url, struct addrinfo **wout);
static int try_socket(struct addrinfo *addrinfo);
static int try_connect(int sockfd, struct addrinfo *addrinfo);

static struct url *parse_url(const char *url);

// fcntl flat setting boilerplate
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static ssize_t net_recv(struct fetch_state *st, void *buf, size_t len) {
    if (!st->is_tls)
        return recv(st->netfd, buf, len, 0);
    else
        return SSL_read(st->ssl, buf, len);
}

static bool handle_http_headers(struct fetch_state *st);
static void handle_http_body_bytes(struct fetch_state *st,
                                   const char *data,
                                   size_t len);
static void handle_http_body(struct fetch_state *st);

static bool flush_pending(struct fetch_state *st);
static void flush_bassoon(struct fetch_state *st);

static void *fetcher(void *arg); 

FILE *fetch(const char *url, const char *init[4]) {
    struct fetch_state *fs = calloc(1, sizeof(struct fetch_state));
    if (!fs) {
        return null(ENOMEM);
    }
    fs->is_tls = strncmp(url, "https://", 8) == 0;
    struct url *URL = parse_url(url);
    if (!URL) {
        return null(errno);
    }
    fs->hostname = strdup(stroff(URL->hostname));

    int sv[2] = {0};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        url_free(URL);
        free(fs);
        return null(errno);
    }
    int app_fd = sv[0];
    int fetch_fd = sv[1];
    set_nonblocking(fetch_fd);

    struct addrinfo *res = 0;
    int rc = tcp_getaddrinfo(URL, &res);
    if (rc != 0) { 
        url_free(URL);
        free(fs);
        return null(errno);
    }

    int sockfd = try_socket(res);
    if (sockfd < 0) { url_free(URL); free(fs); return null(ENOMEM); }
    if (try_connect(sockfd, res) != 0) { url_free(URL); free(fs); return null(ECONNREFUSED); }
    if (fs->is_tls) {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();

        fs->ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!fs->ssl_ctx) {
            close(sockfd);
            close(fetch_fd);
            free(fs);
            return null(errno);
        }

        fs->ssl = SSL_new(fs->ssl_ctx);
        if (!fs->ssl) {
            SSL_CTX_free(fs->ssl_ctx);
            close(sockfd);
            close(fetch_fd);
            free(fs);
            return null(errno);
        }

        SSL_set_fd(fs->ssl, sockfd);
        SSL_set_tlsext_host_name(fs->ssl, fs->hostname);

        int err = SSL_connect(fs->ssl);
        if (err <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(fs->ssl);
            SSL_CTX_free(fs->ssl_ctx);
            close(sockfd);
            close(fetch_fd);
            free(fs);
            return null(errno);
        }
    }

    set_nonblocking(sockfd);

    freeaddrinfo(res);
    str *GET = string(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: yarts/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        stroff(URL->pathname),
        fs->hostname
    );
    url_free(URL);

    if (!GET) {
        close(sockfd);
        return null(errno);
    }

    ssize_t sent = 0;
    if (fs->is_tls) {
        sent = SSL_write(fs->ssl, stroff(GET), len(GET));
    } else {
        sent = send(sockfd, stroff(GET), len(GET), 0);
    }
    free(GET);
    if (sent < 0 && errno != EAGAIN) {
        return null(errno);
    }

    if (!fs) {
        close(sockfd);
        close(fetch_fd);
        return null(ENOMEM);
    }
    fs->netfd = sockfd;
    fs->outfd = fetch_fd;
    fs->ep = epoll_create1(0);
    if (fs->ep < 0) {
        free(fs);
        close(sockfd);
        close(fetch_fd);
        return null(errno);
    }
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = sockfd
    };
    epoll_ctl(fs->ep, EPOLL_CTL_ADD, sockfd, &ev);

    fs->headers_done = false;
    fs->header_len = 0;

    // Initialize body parsing state
    fs->chunked_mode = false;
    fs->current_chunk_size = 0;
    fs->reading_chunk_size = true;
    fs->chunk_line_len = 0;

    if (bhop(fs->bass)) {
        perror("bhop()");
        close(app_fd);
        return NULL;
    }

    fs->http_done = false;

    // spawn background worker thread
    pthread_t tid;
    pthread_create(&tid, NULL, fetcher, fs);

    // detach so it cleans up after finishing
    pthread_detach(tid);

    FILE *fetchfile = fdopen(app_fd, "r");
    if (!fetchfile) {
        perror("fdopen()");
        close(app_fd);
        return NULL;
    }
    return fetchfile;
}

static ssize_t read_full(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = recv(fd, (char*)buf + off, len - off, 0);
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;               // real error
        }
        off += n;
    }
    return off;
}

static char *host(struct url *url) {
    if (!url || !url->hostname) { return null(EINVAL); }

    if (url->port) {
        if (len(url->hostname) > 0 && len(url->port) > 0)
            return string("%s:%s", stroff(url->hostname), stroff(url->port));
    }
    return strdup(stroff(url->hostname));
}

static int url_free(struct url *url) {
    int count = 0;
    if (!url) {
        return count;
    }
    count++;

    if (url->hostname) {
        count += (free(url->hostname), 1);
    }
    if (url->pathname) {
        count += (free(url->pathname), 1);
    }
    if (url->port) {
        count += (free(url->port), 1);
    }
    count += (free(url), 1);
    return count;
}

static int tcp_getaddrinfo(
    struct url *url,
    struct addrinfo **wout
) {
    if (!url || !wout) {
        return EINVAL;
    }
    struct addrinfo hints = {
        .ai_family=AF_INET,
        .ai_socktype=SOCK_STREAM
    };
    int rc = getaddrinfo(stroff(url->hostname), stroff(url->port), &hints, wout);
    if (rc != 0) {
        perror("getaddrinfo");
        url_free(url);
        return rc;
    }
    return 0;
}

static int try_socket(struct addrinfo *addrinfo) {
    // our own definition
    int _ENOMEM_ = -1;
    int sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(addrinfo);
        return _ENOMEM_;
    }
    return sockfd;
}

static int try_connect(int sockfd, struct addrinfo *addrinfo) {
    if (connect(sockfd, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0) {
        perror("connect");
        freeaddrinfo(addrinfo);
        close(sockfd);
        return ECONNREFUSED;
    }
    return 0;
}

static struct url *parse_url(const char *url) {
    CURLU *u = curl_url();
    if (!u) {
        errno = ENOMEM;
        return NULL;
    }

    curl_url_set(u, CURLUPART_URL, url, 0);
    struct url *URL = calloc(1, sizeof(struct url));
    if (!URL) {
        curl_url_cleanup(u);
        errno = ENOMEM;
        return NULL;
    }

    char *host_c = NULL;
    char *path_c = NULL;
    char *port_c = NULL;

    curl_url_get(u, CURLUPART_HOST, &host_c, 0);
    curl_url_get(u, CURLUPART_PATH, &path_c, 0);
    curl_url_get(u, CURLUPART_PORT, &port_c, CURLU_DEFAULT_PORT);

    URL->hostname = string("%s", host_c);
    URL->pathname = string("%s", path_c);
    URL->port = string("%s", port_c);

    curl_free(host_c);
    curl_free(path_c);
    curl_free(port_c);

    curl_url_cleanup(u);
    return URL;
}

static void parse_http_headers(struct fetch_state *st) {
    // Null-terminate header buffer for string scanning
    size_t len = st->header_len;

    // Safety check
    if (len >= sizeof(st->header_buf))
        len = sizeof(st->header_buf) - 1;

    st->header_buf[len] = '\0';

    // Detect Transfer-Encoding: chunked
    if (strcasestr(st->header_buf, "Transfer-Encoding: chunked")) {
        st->chunked_mode = true;
        return;
    }

    // Detect Content-Length
    char *cl = strcasestr(st->header_buf, "Content-Length:");
    if (cl) {
        st->chunked_mode = false;
        st->content_length = strtoul(cl + 15, NULL, 10);
        return;
    }

    // Fallback if neither header is found
    // Many servers default to identity + Connection: close
    st->chunked_mode = false;
    st->content_length = 0;
}

static void handle_http_body_bytes(struct fetch_state *st,
                                   const char *data,
                                   size_t len)
{
    size_t i = 0;

    while (i < len) {
        /* 1. READ THE CHUNK-SIZE LINE */
        if (st->reading_chunk_size) {
            char c = data[i++];

            // Accumulate until CRLF
            if (c == '\r') {
                continue; // skip
            }

            if (c == '\n') {
                // End of chunk-size line
                st->chunk_line[st->chunk_line_len] = '\0';

                // Hex decode the chunk size
                st->current_chunk_size =
                    strtoul(st->chunk_line, NULL, 16);

                st->chunk_line_len = 0;

                if (st->current_chunk_size == 0) {
                    st->http_done = true;
                } else {
                    st->reading_chunk_size = false;
                }

                continue;
            }

            if (st->chunk_line_len < sizeof(st->chunk_line) - 1) {
                st->chunk_line[st->chunk_line_len++] = c;
            }

            continue;
        }

        /* 2. READ CHUNK PAYLOAD */
        if (!st->reading_chunk_size && st->current_chunk_size > 0) {
            size_t available = len - i;
            size_t need = st->current_chunk_size;

            size_t to_copy = (available < need) ? available : need;

            // Feed payload bytes to bassoon parser
            fwrite(data + i, 1, to_copy, st->bass[0]);

            i += to_copy;
            st->current_chunk_size -= to_copy;

            // If not enough bytes to finish payload, exit now
            if (st->current_chunk_size > 0) {
                return;
            }

            // Payload exactly finished expect CRLF next
            // so switch to CRLF-skip mode
            if (st->current_chunk_size == 0) {
                // Next bytes should be "\r\n"
                st->reading_chunk_size = false; // momentary
                st->expecting_crlf = 2;
            }

            continue;
        }

        /* 3. SKIP CRLF AFTER PAYLOAD */
        if (st->expecting_crlf > 0) {
            char c = data[i++];
            if (c == '\r' || c == '\n') {
                st->expecting_crlf--;
                if (st->expecting_crlf == 0) {
                    // Now start the next chunk-size line
                    st->reading_chunk_size = true;
                }
            }
            continue;
        }

        /* Should not reach here */
        i++;
    }
}

static bool handle_http_headers(struct fetch_state *st) {
    char buf[4096];

    for (;;) {
        ssize_t n = net_recv(st, buf, sizeof(buf));

        if (n > 0) {
            // Append to header buffer
            if (st->header_len + n > sizeof(st->header_buf)) {
                // headers too big
                // you can error out or realloc
                return false;
            }

            memcpy(st->header_buf + st->header_len, buf, n);
            st->header_len += n;

            // Check if we have full header: "\r\n\r\n"
            if (st->header_len >= 4) {
                if (memmem(st->header_buf, st->header_len, "\r\n\r\n", 4)) {

                    st->headers_done = true;

                    // OPTIONAL: parse headers here
                    parse_http_headers(st);

                    // Remove header bytes from stream:
                    // find where the header ends
                    char *ptr = memmem(st->header_buf, st->header_len, "\r\n\r\n", 4);
                    size_t header_end = (ptr + 4) - st->header_buf;

                    // Move leftover bytes to body buffer
                    size_t leftover = st->header_len - header_end;

                    // For next step (body), we feed leftover directly
                    if (leftover > 0) {
                        // feed to body parser immediately
                        handle_http_body_bytes(st,
                              st->header_buf + header_end, leftover);
                    }

                    return true; // done with headers
                }
            }

            // CONTINUE LOOP — maybe more header bytes available in nonblocking recv
            continue;
        }

        else if (n == 0) {
            // Server closed unexpectedly before sending full headers
            st->http_done = true;
            return false;
        }

        else { // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data NOW — epoll will wake us again
                return false;
            }
            // real error
            return false;
        }
    }
}

static void handle_http_body(struct fetch_state *st) {
    char buf[4096];

    ssize_t n = net_recv(st, buf, sizeof(buf));

    if (n > 0) {
        // feed raw bytes to chunk/body parser
        handle_http_body_bytes(st, buf, (size_t)n);
        return;
    }

    if (n == 0) {
        // TCP closed — if chunked, this could be abrupt
        st->http_done = true;
        return;
    }

    // n < 0: check errno
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // no data right now — epoll will tell us later
        return;
    }

    // real error
    st->http_done = true;
}

static bool flush_pending(struct fetch_state *st) {
    while (st->pending_len > 0) {
        ssize_t n = send(st->outfd,
                         st->pending_buf + st->pending_off,
                         st->pending_len,
                         0);

        if (n > 0) {
            st->pending_off += (size_t)n;
            st->pending_len -= (size_t)n;
        }
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return false; // still pending
        }
        else {
            // real error — stop producing
            st->http_done = true;
            return false;
        }
    }

    // pending complete: free and reset
    free(st->pending_buf);
    st->pending_buf = NULL;
    st->pending_off = 0;
    st->pending_len = 0;

    return true; // pending fully flushed
}

static void flush_bassoon(struct fetch_state *st) {
    FILE *rd = st->bass[1];
    int out = st->outfd;

    /* 1. Handle pending partial send */
    if (st->pending_len > 0) {
        ssize_t n = send(out,
                         st->pending_buf + st->pending_off,
                         st->pending_len - st->pending_off,
                         MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            st->http_done = true;
            return;
        }

        st->pending_off += n;

        if (st->pending_off < st->pending_len) {
            return;
        }


        free(st->pending_buf);
        st->pending_buf = NULL;
        st->pending_len = 0;
        st->pending_off = 0;
    }

    /* 2. Read NDJSON from Bassoon */
    char *line = NULL;
    size_t cap = 0;
    ssize_t got;


    while ((got = getline(&line, &cap, rd)) != -1) {

        ssize_t sent = send(out, line, got, MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                st->pending_buf = malloc(got);
                memcpy(st->pending_buf, line, got);
                st->pending_len = got;
                st->pending_off = 0;
                free(line);
                return;
            }
            st->http_done = true;
            free(line);
            return;
        }

        if (sent < got) {
            size_t rem = got - sent;
            st->pending_buf = malloc(rem);
            memcpy(st->pending_buf, line + sent, rem);
            st->pending_len = rem;
            st->pending_off = 0;
            free(line);
            return;
        }
    }

    free(line);

    if (feof(rd) && st->http_done && st->pending_len == 0 && !st->closed_outfd) {
        close(out);
        st->closed_outfd = true;
    }
}

static void *fetcher(void *arg) {
    struct fetch_state *fs = arg;
    struct epoll_event events[4];

    /* ---------------------------
       1. MAIN: Read HTTP response
       --------------------------- */
    while (!fs->http_done) {

        int n = epoll_wait(fs->ep, events, 4, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            /* New data from the network */
            if (fd == fs->netfd && (ev & EPOLLIN)) {
                if (!fs->headers_done)
                    handle_http_headers(fs);
                else
                    handle_http_body(fs);
            }
        }
    }

    fclose(fs->bass[0]);
    fs->bass[0] = NULL;

    /* There may still be unflushed parsed objects */
    flush_bassoon(fs);

    fclose(fs->bass[1]);
    fs->bass[1] = NULL;

    if (fs->is_tls) {
        SSL_shutdown(fs->ssl);
        SSL_free(fs->ssl);
        SSL_CTX_free(fs->ssl_ctx);
    }

    if (!fs->closed_outfd) {
        close(fs->outfd);
        fs->closed_outfd = true;
    }

    close(fs->netfd);
    close(fs->ep);

    free(fs->hostname);
    free(fs);

    return NULL;
}
