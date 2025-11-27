#define _GNU_SOURCE
#include "common.h"
#include "fetch.h"
#include <netdb.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

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
static ssize_t read_exact(int netfd,
                          char **bufp, size_t *buflen,
                          void *dst, size_t n);

// Read a CRLF-terminated ASCII line (no CRLF in result)
static int read_line(int netfd,
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

    struct addrinfo *res = 0;
    int rc = tcp_getaddrinfo(URL, &res);
    if (rc != 0) { return neg1(errno); }

    int sockfd = try_socket(res);
    if (sockfd < 0) { url_free(URL); return neg1(ENOMEM); }
    if (try_connect(sockfd, res) != 0) { url_free(URL); return neg1(ECONNREFUSED); }

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
    if (sent < 0) {
        return neg1(errno);
    }
    parse_headers_and_forward(sockfd, fetch_fd);

    close(sockfd);
    close(fetch_fd);

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
static ssize_t read_exact(int netfd,
                          char **bufp, size_t *buflen,
                          void *dst, size_t n)
{
    size_t copied = 0;
    char *dstc = dst;

    // Consume from leftover first
    while (*buflen > 0 && copied < n) {
        size_t take = (*buflen < (n - copied)) ? *buflen : (n - copied);
        memcpy(dstc + copied, *bufp, take);
        *bufp += take;
        *buflen -= take;
        copied += take;
    }

    // Read remaining bytes from network
    while (copied < n) {
        ssize_t r = recv(netfd, dstc + copied, n - copied, 0);
        if (r <= 0) return r; // error or EOF
        copied += r;
    }

    return copied;
}

// Read a CRLF-terminated ASCII line (no CRLF in result)
static int read_line(int netfd,
                     char **bufp, size_t *buflen,
                     char *out, size_t outcap)
{
    size_t outlen = 0;

    for (;;) {
        // If leftover buffer has data
        if (*buflen > 0) {
            // copy one byte
            char c = **bufp;
            (*bufp)++;
            (*buflen)--;

            if (c == '\n') {     // End of line
                out[outlen] = 0;
                return 1;
            }

            if (c != '\r') {     // skip CR
                if (outlen + 1 < outcap)
                    out[outlen++] = c;
            }

            continue;
        }

        // If no leftover, read more from network
        char tmp[1];
        ssize_t r = recv(netfd, tmp, 1, 0);
        if (r <= 0) return 0;

        char c = tmp[0];
        if (c == '\n') {
            out[outlen] = 0;
            return 1;
        }
        if (c != '\r') {
            if (outlen + 1 < outcap)
                out[outlen++] = c;
        }
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
    for (;;) {
        // 1. Read chunk-size line
        char line[64];
        if (!read_line(netfd, bufp, buflen, line, sizeof line))
            return -1;

        // Parse hex
        size_t chunk_size = strtoul(line, NULL, 16);

        if (chunk_size == 0) {
            // Final chunk → read trailing CRLF and stop
            read_crlf(netfd, bufp, buflen);
            return 0;
        }

        // 2. Read chunk payload
        while (chunk_size > 0) {
            char buf[4096];
            size_t to_read = chunk_size < sizeof buf ? chunk_size : sizeof buf;

            ssize_t n = read_exact(netfd, bufp, buflen, buf, to_read);
            if (n <= 0) return -1;

            send(outfd, buf, n, 0);
            chunk_size -= n;
        }

        // 3. Consume trailing CRLF
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

