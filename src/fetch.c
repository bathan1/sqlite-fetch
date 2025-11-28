#define _GNU_SOURCE
#include "common.h"
#include "clarinet.h"
#include "fetch.h"
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

    /* --- HTTP HEADER PARSING --- */
    bool headers_done;
    char header_buf[8192];  // store header bytes
    size_t header_len;

    /* --- TRANSFER MODE --- */
    bool chunked_mode;      // true if "Transfer-Encoding: chunked"
    size_t content_length;  // only used if not chunked

    /* --- CHUNKED DECODING STATE --- */
    bool reading_chunk_size;    // true = reading hex size line
    char chunk_line[128];       // buffer for chunk-size line
    size_t chunk_line_len;      // how many chars collected
    size_t current_chunk_size;  // remaining bytes in current chunk
    int expecting_crlf;         // 2 -> expecting "\r\n"

    /* --- CLARINET JSON PARSER --- */
    clarinet_t *clare;

    /* --- NONBLOCKING SEND STATE FOR outfd --- */
    char *pending_send;         // partial write buffer (JSON object)
    size_t pending_len;         // bytes left to send
    size_t pending_off;         // current offset in pending_send

    /* --- TERMINATION STATE --- */
    bool http_done;             // reached end of chunked stream or TCP closed
    bool closed_outfd;          // have we closed outfd yet?
};

/**
 * Taken from Web API URL, word 4 word, bar 4 bar.
 */
struct url_s {
    /**
     * A string containing the domain of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL}
     */
    buffer *hostname;

    /**
     * A string containing an initial '/' followed by the path of the URL, 
     * not including the query string or fragment.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/pathname}
     */
    buffer *pathname;

    /**
     * A string containing the port number of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/port}
     */
    buffer *port;
};

/** Alias for {@link struct url_s} */
typedef struct url_s url_t;
static char *host(url_t *url);

static int url_free(struct url_s *url);
static int tcp_getaddrinfo(struct url_s *url, struct addrinfo **wout);
static int try_socket(struct addrinfo *addrinfo);
static int try_connect(int sockfd, struct addrinfo *addrinfo);
static int try_send(int sockfd, char *buffer, size_t length, int flags);

// Read exactly N bytes, pulling from leftover buffer first.
static ssize_t read_exact(int fd, char **bufp, size_t *buflen,
                          char *out, size_t want);

// Read a CRLF-terminated ASCII line (no CRLF in result)
static bool read_line(int netfd,
                     char **bufp, size_t *buflen,
                     char *out, size_t outcap);

static int read_crlf(int netfd, char **bufp, size_t *buflen);
// Decode chunked body and forward
static int forward_chunked(int netfd, int outfd,
                           char **bufp, size_t *buflen);
// Content-Length forwarding
static int forward_content_length(int netfd, int outfd,
                                  char **bufp, size_t *buflen,
                                  size_t content_len);
// Fallback: read until EOF
static int forward_until_close(int netfd, int outfd,
                               char **bufp, size_t *buflen);
static int parse_headers_and_forward(int netfd, int outfd);
static struct url_s *parse_url(const char *url);

// fcntl flat setting boilerplate
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static bool handle_http_headers(struct fetch_state *st);
static void handle_http_body_bytes(struct fetch_state *st,
                                   const char *data,
                                   size_t len);
static void handle_http_body(struct fetch_state *st) {
    char buf[4096];

    ssize_t n = recv(st->netfd, buf, sizeof(buf), 0);

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
                         st->pending_send + st->pending_off,
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
    free(st->pending_send);
    st->pending_send = NULL;
    st->pending_off = 0;
    st->pending_len = 0;

    return true; // pending fully flushed
}

static void flush_json_queue(struct fetch_state *st) {
    /* 1. First, flush any partially-sent object. */
    if (st->pending_len > 0) {
        if (!flush_pending(st))
            return;  // still blocked
    }

    /* 2. Now flush new items from Clarinet's queue */
    while (st->clare->count > 0) {
        char *obj = cqpop(st->clare);
        size_t len = strlen(obj);

        /* Build the wire format: [8-byte length][json bytes] */
        size_t total = 8 + len;
        char *buf = malloc(total);
        if (!buf) {
            free(obj);
            st->http_done = true;
            return;
        }

        uint64_t n64 = len;
        memcpy(buf, &n64, 8);
        memcpy(buf + 8, obj, len);
        free(obj);

        /* Attempt to send the whole thing */
        ssize_t n = send(st->outfd, buf, total, 0);

        if (n == (ssize_t)total) {
            free(buf);
            continue; // go to next object
        }

        if (n >= 0) {
            /* Partial write → store remainder */
            st->pending_send = buf;
            st->pending_off = (size_t)n;
            st->pending_len = total - (size_t)n;
            return;
        }

        /* send() returned -1 */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            st->pending_send = buf;
            st->pending_off = 0;
            st->pending_len = total;
            return;
        }

        /* real error */
        free(buf);
        st->http_done = true;
        return;
    }

    /* 3. If parser is done AND no queue items AND no pending sends → close */
    if (st->http_done &&
        st->clare->count == 0 &&
        st->pending_len == 0 &&
        !st->closed_outfd) {

        close(st->outfd);
        st->closed_outfd = true;
    }
}

static void *fetch_worker_thread(void *arg) {
    struct fetch_state *fs = arg;

    struct epoll_event events[4];

    while (!fs->http_done) {
        int n = epoll_wait(fs->ep, events, 4, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == fs->netfd && (ev & EPOLLIN)) {
                if (!fs->headers_done)
                    handle_http_headers(fs);
                else
                    handle_http_body(fs);
            }

            if (fd == fs->outfd && (ev & EPOLLOUT)) {
                flush_json_queue(fs);
            }
        }
    }

    // cleanup
    close(fs->netfd);
    close(fs->outfd);
    close(fs->ep);

    cqfree(fs->clare);
    free(fs);

    return NULL;
}


int fetch(const char *url) {
    struct url_s *URL = parse_url(url);
    if (!URL) {
        return neg1(errno);
    }

    int sv[2] = {0};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        return neg1(errno);
    }
    int app_fd = sv[0];
    int fetch_fd = sv[1];
    set_nonblocking(fetch_fd);

    struct addrinfo *res = 0;
    int rc = tcp_getaddrinfo(URL, &res);
    if (rc != 0) { return neg1(errno); }

    int sockfd = try_socket(res);
    if (sockfd < 0) { url_free(URL); return neg1(ENOMEM); }
    if (try_connect(sockfd, res) != 0) { url_free(URL); return neg1(ECONNREFUSED); }
    set_nonblocking(sockfd);

    freeaddrinfo(res);
    char *host_value = host(URL);
    buffer *GET = string(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: yarts/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        URL->pathname,
        host_value
    );
    url_free(URL);
    free(host_value);

    if (!GET) {
        close(sockfd);
        return neg1(errno);
    }
    ssize_t sent = send(sockfd, GET, len(GET), 0);
    free(GET);
    if (sent < 0 && errno != EAGAIN) {
        return neg1(errno);
    }

    struct fetch_state *fs = calloc(1, sizeof(struct fetch_state));
    if (!fs) {
        close(sockfd);
        close(fetch_fd);
        return neg1(ENOMEM);
    }
    fs->netfd = sockfd;
    fs->outfd = fetch_fd;
    fs->ep = epoll_create1(0);
    if (fs->ep < 0) {
        free(fs);
        close(sockfd);
        close(fetch_fd);
        return neg1(errno);
    }
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = sockfd
    };
    epoll_ctl(fs->ep, EPOLL_CTL_ADD, sockfd, &ev);

    struct epoll_event ev2 = {
        .events = EPOLLOUT,
        .data.fd = fetch_fd
    };
    epoll_ctl(fs->ep, EPOLL_CTL_ADD, fetch_fd, &ev2);

    fs->headers_done = false;
    fs->header_len = 0;

    // Initialize body parsing state
    fs->chunked_mode = false;
    fs->current_chunk_size = 0;
    fs->reading_chunk_size = true;
    fs->chunk_line_len = 0;

    // Initialize clarinet
    fs->clare = use_clarinet();
    setvbuf(fs->clare->writable, NULL, _IONBF, 0);
    fs->http_done = false;

    // spawn background worker thread
    pthread_t tid;
    pthread_create(&tid, NULL, fetch_worker_thread, fs);

    // detach so it cleans up after finishing
    pthread_detach(tid);


    // parse_headers_and_forward(sockfd, fetch_fd);
    //
    // close(sockfd);
    // close(fetch_fd);
    //
    return app_fd;
}


static char *host(url_t *url) {
    if (!url || !url->hostname) { return null(EINVAL); }

    if (url->port) {
        if (len(url->hostname) > 0 && len(url->port) > 0)
            return string("%s:%s", url->hostname, url->port);
    }
    return strdup(url->hostname);
}

static int url_free(struct url_s *url) {
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
    struct url_s *url,
    struct addrinfo **wout
) {
    if (!url || !wout) {
        return EINVAL;
    }
    struct addrinfo hints = {
        .ai_family=AF_INET,
        .ai_socktype=SOCK_STREAM
    };
    int rc = getaddrinfo(url->hostname, url->port, &hints, wout);
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

static int try_send(int sockfd, char *buffer, size_t length, int flags) {
    ssize_t sent = send(sockfd, buffer, length, 0);
    if (sent < 0) {
        perror("send");
        free(buffer);
        close(sockfd);
        return neg1(errno);
    }
    return sent;
} 

// Read exactly N bytes, pulling from leftover buffer first.
static ssize_t read_exact(int fd, char **bufp, size_t *buflen,
                          char *out, size_t want)
{
    size_t pos = 0;

    /* Consume leftover first */
    while (*buflen > 0 && pos < want) {
        out[pos++] = **bufp;
        (*bufp)++;
        (*buflen)--;
    }

    while (pos < want) {
        ssize_t n = recv(fd, out + pos, want - pos, 0);
        if (n <= 0) return -1;
        pos += n;
    }

    return pos;
}

// Read a CRLF-terminated ASCII line (no CRLF in result)
static bool read_line(int fd, char **bufp, size_t *buflen,
                      char *out, size_t outcap)
{
    size_t pos = 0;

    for (;;) {
        /* First use leftover buffer if any */
        while (*buflen > 0) {
            char c = **bufp;

            (*bufp)++;
            (*buflen)--;

            if (pos + 1 < outcap)
                out[pos++] = c;

            if (c == '\n') {       // found end of line
                out[pos] = '\0';
                return true;
            }
        }

        /* Need to read more */
        char tmp[4096];
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) return false;

        *bufp = malloc(n);
        memcpy(*bufp, tmp, n);
        *buflen = n;
    }
}

static int read_crlf(int netfd, char **bufp, size_t *buflen)
{
    char tmp[2];
    return read_exact(netfd, bufp, buflen, tmp, 2) == 2;
}

// Decode chunked body and forward
static int forward_chunked(int netfd, int outfd,
                           char **bufp, size_t *buflen)
{
    clarinet_t *clare = use_clarinet();

    for (;;) {
        char line[64];
        if (!read_line(netfd, bufp, buflen, line, sizeof line))
            return -1;

        size_t chunk_size = strtoul(line, NULL, 16);

        if (chunk_size == 0) {
            read_crlf(netfd, bufp, buflen);

            // Flush remaining objects
            while (clare->count > 0) {
                fflush(clare->writable);
                char *obj = cqpop(clare);
                uint64_t len = strlen(obj);

                send(outfd, &len, sizeof len, 0);
                send(outfd, obj, len, 0);

                free(obj);
            }

            cqfree(clare);
            return 0;
        }

        // Read chunk payload
        while (chunk_size > 0) {
            char buf[4096];
            size_t to_read = chunk_size < sizeof buf ? chunk_size : sizeof buf;

            ssize_t n = read_exact(netfd, bufp, buflen, buf, to_read);
            if (n <= 0) return -1;

            chunk_size -= n;

            // Feed bytes into clarinet's FILE*
            fwrite(buf, n, 1, clare->writable);
            fflush(clare->writable);
            printf("clare size %lu\n", clare->count);
        }

        size_t i = 0;
        // Flush any COMPLETED JSON objects
        while (clare->count > 0) {
            printf("spin %lu, count %lu\n", ++i, clare->count);
            char *obj = cqpop(clare);
            uint64_t len = strlen(obj);
            send(outfd, &len, sizeof(len), 0);
            send(outfd, obj, len, 0);
            free(obj);
        }

        if (!read_crlf(netfd, bufp, buflen))
            return -1;
    }
}

// Content-Length forwarding
static int forward_content_length(int netfd, int outfd,
                                  char **bufp, size_t *buflen,
                                  size_t content_len)
{
    // First, consume leftover bytes
    if (*buflen > 0) {
        size_t to_take = (*buflen < content_len) ? *buflen : content_len;
        send(outfd, *bufp, to_take, 0);
        *bufp += to_take;
        *buflen -= to_take;
        content_len -= to_take;
    }

    // Then read from network
    char buf[4096];
    while (content_len > 0) {
        size_t to_read = content_len < sizeof buf ? content_len : sizeof buf;

        ssize_t n = recv(netfd, buf, to_read, 0);
        if (n <= 0) return -1;

        send(outfd, buf, n, 0);
        content_len -= n;
    }

    return 0;
}

// Fallback: read until EOF
static int forward_until_close(int netfd, int outfd,
                               char **bufp, size_t *buflen)
{
    if (*buflen > 0) {
        send(outfd, *bufp, *buflen, 0);
        *bufp = NULL;
        *buflen = 0;
    }
    char buf[4096];
    ssize_t n;
    while ((n = recv(netfd, buf, sizeof buf, 0)) > 0) {
        send(outfd, buf, n, 0);
    }
    return 0;
}

static int parse_headers_and_forward(int netfd, int outfd)
{
    char hdrbuf[8192];
    size_t hdrlen = 0;

    for (;;) {
        ssize_t n = recv(netfd, hdrbuf + hdrlen, sizeof(hdrbuf) - hdrlen, 0);
        if (n <= 0) return -1;
        hdrlen += n;

        char *p = memmem(hdrbuf, hdrlen, "\r\n\r\n", 4);
        if (p) {
            size_t header_length = (p - hdrbuf) + 4;

            size_t body_left = hdrlen - header_length;
            char *body_ptr = hdrbuf + header_length;

            bool is_chunked = false;
            size_t content_len = 0;

            char *headers = malloc(header_length + 1);
            memcpy(headers, hdrbuf, header_length);
            headers[header_length] = 0;

            if (strcasestr(headers, "transfer-encoding: chunked"))
                is_chunked = true;

            char *cl = strcasestr(headers, "content-length:");
            if (cl) content_len = strtoul(cl + 15, NULL, 10);

            free(headers);

            if (is_chunked) {
                return forward_chunked(netfd, outfd, &body_ptr, &body_left);
            }

            if (content_len > 0) {
                return forward_content_length(netfd, outfd, &body_ptr, &body_left, content_len);
            }

            return forward_until_close(netfd, outfd, &body_ptr, &body_left);
        }
    }
}


static struct url_s *url_free_error(struct url_s *u) {
    if (!u) return NULL;
    if (u->hostname)     free(u->hostname);
    if (u->pathname) free(u->pathname);
    if (u->port)     free(u->port);
    free(u);
    return NULL;
}

static struct url_s *parse_url(const char *url) {
    CURLU *u = curl_url();
    if (!u) {
        errno = ENOMEM;
        return NULL;
    }

    curl_url_set(u, CURLUPART_URL, url, 0);
    struct url_s *URL = calloc(1, sizeof(struct url_s));
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
                    // FINAL CHUNK
                    st->http_done = true;
                } else {
                    // Switch to reading payload
                    st->reading_chunk_size = false;
                }

                continue;
            }

            // Normal hex digit or extension
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

            // Feed payload bytes to Clarinet parser
            fwrite(data + i, 1, to_copy, st->clare->writable);

            i += to_copy;
            st->current_chunk_size -= to_copy;

            // If not enough bytes to finish payload, exit now
            if (st->current_chunk_size > 0)
                return;

            // Payload exactly finished → expect CRLF next
            // so switch to CRLF-skip mode
            if (st->current_chunk_size == 0) {
                // Next bytes should be "\r\n"
                st->reading_chunk_size = false; // momentary
                st->expecting_crlf = 2;        // NEW STATE WE ADD
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
        ssize_t n = recv(st->netfd, buf, sizeof(buf), 0);

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
