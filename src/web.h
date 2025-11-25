#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

/** Sets {@link errno} to ERROR_CODE so you don't have to every time. */
void *null(int error_code) {
    errno = error_code;
    return NULL;
}

long long zero (int error_code) {
    return (long long) null(error_code);
}

long long neg1 (int error_code) {
    return zero(error_code) - 1;
}

struct buffer_s {
    size_t length;
    char *bytes;
};
typedef struct buffer_s buffer_t;

int buffer_free(struct buffer_s *buffer) {
    int count = 0;
    if (!buffer) {
        return count;
    }
    count++;

    if (buffer->bytes) {
        free(buffer->bytes);
        count++;
    }
    free(buffer);
    return count;
}

struct buffer_s *buffer_alloc() {
    struct buffer_s *buffer = malloc(sizeof(struct buffer_s));
    if (!buffer) {
        return null(ENOMEM);
    }
    buffer->length = 1;
    buffer->bytes = calloc(1, sizeof(char));
    return buffer;
}

/**
 * "Move" ownership of STRING to returned BUFFER. If LENGTH
 * is 0, then `strlen` is computed on STRING.
 */
struct buffer_s *buffer_mv_string(char *string, int length) {
    struct buffer_s *buffer = buffer_alloc();
    if (!buffer) {
        return null(ENOMEM);
    }
    buffer->length = length > 0 ? length : strlen(string);
    buffer->bytes = string;
    return buffer;
}

/**
 * Shamelessly stolen from Web API URL.
 */
struct url_s {
    /**
     * A string containing the domain of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL}
     */
    struct buffer_s *host;

    /**
     * A string containing an initial '/' followed by the path of the URL, 
     * not including the query string or fragment.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/pathname}
     */
    struct buffer_s *pathname;

    /**
     * A string containing the port number of the URL.
     *
     * {@link https://developer.mozilla.org/en-US/docs/Web/API/URL/port}
     */
    struct buffer_s *port;
};
/** Alias for {@link struct url_s} */
typedef struct url_s url_t;

int url_free(struct url_s *url) {
    int count = 0;
    if (!url) {
        return count;
    }
    count++;

    if (url->host) {
        count += buffer_free(url->host);
    }
    if (url->pathname) {
        count += buffer_free(url->pathname);
    }
    return count;
}
struct url_s *url_alloc() {
    struct url_s *url = malloc(sizeof(struct url_s));
    if (!url) {
        return null(ENOMEM);
    }
    url->host = buffer_alloc();
    if (!url->host) {
        url_free(url);
        return null(ENOMEM);
    }
    url->pathname = buffer_alloc();
    if (!url->pathname) {
        url_free(url);
        return null(ENOMEM);
    }
    return url;
}

char *string(const char *fmt, ...) {
    va_list ap;
    va_list ap2;

    // First pass: determine required size
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) {
        va_end(ap2);
        return NULL;
    }

    char *buf = malloc(needed + 1);
    if (!buf) {
        va_end(ap2);
        return NULL;
    }

    // Second pass: actually write
    vsnprintf(buf, needed + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

