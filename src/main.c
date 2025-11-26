#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <yajl/yajl_parse.h>
#include <stdlib.h>

//
// ====================== JSON OBJECT BUILDER ======================
// Streams one top-level JSON object at a time
//

struct builder_s {
    char *json;
    size_t len;
    size_t cap;

    int depth;
    bool complete;
};

static void builder_reserve(struct builder_s *b, size_t extra) {
    if (b->len + extra >= b->cap) {
        size_t newcap = (b->cap == 0 ? 256 : b->cap * 2);
        while (newcap < b->len + extra) newcap *= 2;
        b->json = realloc(b->json, newcap);
        b->cap  = newcap;
    }
}

static void builder_put(struct builder_s *b, const char *s, size_t n) {
    builder_reserve(b, n);
    memcpy(b->json + b->len, s, n);
    b->len += n;
}

static void builder_ch(struct builder_s *b, char c) {
    builder_put(b, &c, 1);
}

static void init_builder(struct builder_s *b) {
    b->json = NULL;
    b->len = 0;
    b->cap = 0;
    b->depth = 0;
    b->complete = false;
}

static void reset_builder(struct builder_s *b) {
    b->len = 0;
    b->depth = 0;
    b->complete = false;
}

//
// ====================== YAJL CALLBACKS ======================
//

static int handle_start_map(void *ctx) {
    struct builder_s *b = ctx;
    b->depth++;
    if (b->depth == 1) reset_builder(b);
    builder_ch(b, '{');
    return 1;
}

static int handle_end_map(void *ctx) {
    struct builder_s *b = ctx;
    builder_ch(b, '}');
    if (b->depth == 1) b->complete = true;
    b->depth--;
    return 1;
}

static int handle_map_key(void *ctx, const unsigned char *s, size_t len) {
    struct builder_s *b = ctx;
    builder_ch(b, '"');
    builder_put(b, (const char*)s, len);
    builder_ch(b, '"');
    builder_ch(b, ':');
    return 1;
}

static int handle_string(void *ctx, const unsigned char *s, size_t len) {
    struct builder_s *b = ctx;
    builder_ch(b, '"');
    builder_put(b, (const char*)s, len);
    builder_ch(b, '"');
    return 1;
}

static int handle_number(void *ctx, const char *s, size_t len) {
    struct builder_s *b = ctx;
    builder_put(b, s, len);
    return 1;
}

static int handle_boolean(void *ctx, int v) {
    struct builder_s *b = ctx;
    if (v) builder_put(b, "true", 4);
    else   builder_put(b, "false", 5);
    return 1;
}

static int handle_null(void *ctx) {
    struct builder_s *b = ctx;
    builder_put(b, "null", 4);
    return 1;
}

static int handle_start_array(void *ctx) {
    struct builder_s *b = ctx;
    b->depth++;
    // (We don't output outer [ ])
    return 1;
}

static int handle_end_array(void *ctx) {
    struct builder_s *b = ctx;
    b->depth--;
    return 1;
}

static yajl_callbacks callbacks = {
    .yajl_null = handle_null,
    .yajl_boolean = handle_boolean,
    .yajl_integer = NULL,
    .yajl_double = NULL,
    .yajl_number = handle_number,
    .yajl_string = handle_string,
    .yajl_start_map = handle_start_map,
    .yajl_map_key = handle_map_key,
    .yajl_end_map = handle_end_map,
    .yajl_start_array = handle_start_array,
    .yajl_end_array = handle_end_array
};

//
// ====================== JSON STREAM PARSER ======================
//

static void parse_json_stream(int fd, int outfd) {
    char buf[4096];
    ssize_t n;

    struct builder_s builder;
    init_builder(&builder);

    yajl_handle hand = yajl_alloc(&callbacks, NULL, &builder);

    while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        yajl_status st = yajl_parse(hand, (unsigned char*)buf, (size_t)n);

        if (st == yajl_status_error) {
            fprintf(stderr, "JSON parse error\n");
            break;
        }

        if (builder.complete) {
            write(outfd, builder.json, builder.len);
            write(outfd, "\n", 1);
            reset_builder(&builder);
        }
    }

    yajl_complete_parse(hand);
    yajl_free(hand);
}

//
// ====================== HEADER PARSING ======================
//

static bool detect_chunked(const char *hdr, size_t len) {
    for (size_t i = 0; i + 17 < len; i++) {
        if (strncasecmp(hdr + i, "Transfer-Encoding:", 18) == 0) {
            const char *p = hdr + i + 18;
            while (*p == ' ' || *p == '\t') p++;
            if (strncasecmp(p, "chunked", 7) == 0)
                return true;
        }
    }
    return false;
}

static int skip_http_headers(int fd,
                             char **body_out,
                             size_t *body_len_out,
                             char **hdr_out,
                             size_t *hdr_len_out)
{
    static char buf[16384];
    size_t len = 0;

    for (;;) {
        ssize_t n = recv(fd, buf + len, sizeof(buf) - len, 0);
        if (n <= 0) return -1;

        len += n;

        char *p = memmem(buf, len, "\r\n\r\n", 4);
        if (p) {
            size_t hdr_len = (p - buf) + 4;
            *hdr_out      = buf;
            *hdr_len_out  = hdr_len;

            *body_out     = buf + hdr_len;
            *body_len_out = len - hdr_len;

            return 0;
        }
    }
}

//
// ====================== CHUNK DECODER ======================
//

static ssize_t read_line(int fd, char *buf, size_t max) {
    size_t pos = 0;

    while (pos < max - 1) {
        ssize_t n = recv(fd, buf + pos, 1, 0);
        if (n <= 0) return n;

        if (buf[pos] == '\n') {
            buf[pos+1] = 0;
            return pos+1;
        }
        pos++;
    }
    return -1;
}

static void forward_chunked(int netfd,
                            int outfd,
                            const char *initial,
                            size_t initial_len)
{
    // initial leftover is part of first chunk payload
    if (initial_len > 0)
        write(outfd, initial, initial_len);

    char line[128];
    char buf[4096];

    for (;;) {
        ssize_t ln = read_line(netfd, line, sizeof(line));
        if (ln <= 0) return;

        size_t chunklen = strtoul(line, NULL, 16);
        if (chunklen == 0) {
            read_line(netfd, line, sizeof(line));
            return;
        }

        size_t remaining = chunklen;

        while (remaining > 0) {
            size_t want = remaining < sizeof(buf) ? remaining : sizeof(buf);
            ssize_t n = recv(netfd, buf, want, 0);
            if (n <= 0) return;
            write(outfd, buf, n);
            remaining -= n;
        }

        recv(netfd, line, 2, 0);
    }
}

//
// ====================== MAIN ======================
//

int main(void) {
    const char *host = "jsonplaceholder.typicode.com";
    const char *path = "/todos";

    // Resolve DNS
    struct addrinfo hints = {0}, *res;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    if (getaddrinfo(host, "80", &hints, &res) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) { perror("socket"); return 1; }

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        perror("connect");
        return 1;
    }
    freeaddrinfo(res);

    // Send request
    char req[1024];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: fetch/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host
    );

    send(sock, req, req_len, 0);

    // Parse headers
    char *body_ptr = NULL;
    size_t body_len = 0;
    char *hdr = NULL;
    size_t hdr_len = 0;

    if (skip_http_headers(sock,
                          &body_ptr, &body_len,
                          &hdr, &hdr_len) != 0)
    {
        fprintf(stderr, "header read error\n");
        return 1;
    }

    bool is_chunked = detect_chunked(hdr, hdr_len);

    // Create a streaming FD for the JSON parser
    int sp[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sp);
    int feeder = sp[0];
    int parser = sp[1];

    if (is_chunked) {
        forward_chunked(sock, feeder, body_ptr, body_len);
    } else {
        write(feeder, body_ptr, body_len);
        char buf[4096];
        ssize_t n;
        while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
            write(feeder, buf, n);
    }

    close(feeder);
    close(sock);

    // Parse each complete JSON object
    parse_json_stream(parser, STDOUT_FILENO);

    close(parser);
    return 0;
}

