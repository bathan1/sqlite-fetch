#define _GNU_SOURCE

#include "tcp.h"
#include "fetch.h"
#include "cfns.h"

#include <netdb.h>
#include <openssl/types.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/epoll.h>

void url_free(struct url *url) {
    if (!url) {
        return;
    }

    if (url->host) free(url->host);
    if (url->protocol) free(url->protocol);
    if (url->hostname) free(url->hostname);
    if (url->pathname) free(url->pathname);
    if (url->port) free(url->port);
}

void dispatch_free(struct dispatch *dispatch) {
    // CALLER frees sockfd
    if (!dispatch) return;
    url_free(&dispatch->url);

    if (dispatch->addrinfo) {
        freeaddrinfo(dispatch->addrinfo);
        dispatch->addrinfo = NULL;
    }
    free(dispatch);
    // if (dispatch->ssl || dispatch->ctx) {
    //     tls_free(dispatch->ssl, dispatch->ctx);
    // }
}

static struct url *url_of_string(const char *url);
struct dispatch *fetch_socket(const char *url, const char *init[4]) {
    struct dispatch *disp = calloc(1, sizeof(struct dispatch));
    if (!disp) {
        return perror_rc(NULL, "calloc()", 0);
    }

    struct url *URL = url_of_string(url);
    if (!URL) {
        return perror_rc(NULL, "url_of_string()", free(disp));
    }
    disp->url = *URL;
    if (tcp_getaddrinfo(str(disp->url.hostname), str(disp->url.port), &disp->addrinfo)) {
        return perror_rc(NULL, "tcp_getaddrinfo()", dispatch_free(disp));
    }

    disp->sockfd = tcp_socket(disp->addrinfo);
    if (disp->sockfd < 0) {
        return perror_rc(NULL, "tcp_socket()", dispatch_free(disp));
    }
    // just free the head, we need to keep the values alive
    // in dispatch
    free(URL);
    return disp;
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0;
}
int use_fetch(int fds[4], struct dispatch *dispatch) {
    // if (connect(dispatch->sockfd, dispatch->addrinfo->ai_addr, dispatch->addrinfo->ai_addrlen) < 0) {
    //     return perror_rc(-1, "connect()", close(dispatch->sockfd), dispatch_free(dispatch));
    // }
    // if (is_tls) {
    //     if (tls_connect(&dispatch->ssl, dispatch->sockfd, &dispatch->ctx, str(dispatch->url.hostname))) {
    //         return perror_rc(-1, "tls_connect()", close(dispatch->sockfd), dispatch_free(dispatch));
    //     } // else { ok! }
    // }
    bool is_tls = strncmp(str(dispatch->url.protocol), "https:", 6) == 0;
    SSL **ssl = is_tls ? &dispatch->ssl : NULL;
    SSL_CTX **ctx = is_tls ? &dispatch->ctx : NULL;
    const char *hostname = is_tls ? str(dispatch->url.hostname) : NULL;
    if (ttcp_connect(
        dispatch->sockfd, dispatch->addrinfo->ai_addr, dispatch->addrinfo->ai_addrlen,
        ssl, ctx, hostname) < 0) {
        return perror_rc(-1, "ttcp_connect()",
            close(dispatch->sockfd),
            dispatch_free(dispatch)
        );
    }

    // make recv() nonblocking
    if (set_nonblocking(dispatch->sockfd) < 0) {
        return perror_rc(-1, "set_nonblocking()", close(dispatch->sockfd), dispatch_free(dispatch));
    }
    pre *GET = prefix(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: yarts/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        str(dispatch->url.pathname),
        str(dispatch->url.host)
    );
    if (!GET) {
        return perror_rc(-1, "prefix()", close(dispatch->sockfd), dispatch_free(dispatch));
    }

    if (ttcp_send(dispatch->sockfd, str(GET), len(GET), is_tls ? *ssl : NULL) < 0) {
        return perror_rc(-1, "ttcp_send()", free(GET), close(dispatch->sockfd), dispatch_free(dispatch));
    }

    // if (send_maybe_tls(dispatch->ssl, dispatch->sockfd, str(GET), len(GET)) < 0) {
    //     if (errno != EAGAIN) {
    //         return perror_rc(-1, "send_maybe_tls()", free(GET), close(dispatch->sockfd), dispatch_free(dispatch));
    //     }
    //     // TODO?
    // }
    free(GET);

    int sv[2] = {0};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        return perror_rc(-1, "socketpair()", close(dispatch->sockfd), dispatch_free(dispatch));
    }
    int appfd = sv[0];
    int fetchfd = sv[1];
    if (set_nonblocking(fetchfd)) {
        return perror_rc(-1, "set_nonblocking()", close(appfd), close(fetchfd), close(dispatch->sockfd), dispatch_free(dispatch));

    }

    int pollfd = epoll_create1(0);
    if (pollfd < 0) {
        return perror_rc(-1, "epoll_create1()", close(appfd), close(fetchfd), close(dispatch->sockfd), dispatch_free(dispatch));
    }
    struct epoll_event ev = { .events=EPOLLIN, .data.fd=dispatch->sockfd };
    if (epoll_ctl(pollfd, EPOLL_CTL_ADD, dispatch->sockfd, &ev)) {
        return perror_rc(-1, "epoll_create1()", close(pollfd), close(appfd), close(fetchfd), close(dispatch->sockfd), dispatch_free(dispatch));
    }

    fds[0] = dispatch->sockfd;
    fds[1] = appfd;
    fds[2] = fetchfd;
    fds[3] = pollfd;
    return 0;
}

static bool handle_http_headers(struct fetch_state *st);
static void handle_http_body_bytes(struct fetch_state *st,
                                   const char *data,
                                   size_t len);
static void handle_http_body(struct fetch_state *st);

static bool flush_pending(struct fetch_state *st);
static void flush_bassoon(struct fetch_state *st);

void *fetcher(void *arg) {
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

    ttcp_tls_free(fs->ssl, fs->ssl_ctx);

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

static struct url *url_of_string(const char *url) {
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

    bool is_tls = strncmp(url, "https://", 8) == 0;
    char *host_c = NULL;
    char *path_c = NULL;
    char *port_c = NULL;

    curl_url_get(u, CURLUPART_HOST, &host_c, 0);
    curl_url_get(u, CURLUPART_PATH, &path_c, 0);
    curl_url_get(u, CURLUPART_PORT, &port_c, CURLU_DEFAULT_PORT);

    URL->host = prefix("%s:%s", host_c, port_c);
    URL->hostname = prefix("%s", host_c);
    URL->pathname = prefix("%s", path_c);
    URL->port = prefix("%s", port_c);
    URL->protocol = prefix("%s", is_tls ? "https:" : "http:");


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
        ssize_t n = ttcp_recv(st->netfd, buf, sizeof(buf), st->ssl);
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

    ssize_t n = ttcp_recv(st->netfd, buf, sizeof(buf), st->ssl);
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
