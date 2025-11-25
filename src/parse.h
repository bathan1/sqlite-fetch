#ifndef FETCH_PARSE_H
#define FETCH_PARSE_H

#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char *to_lower(char *s, size_t len) {
    char *lowercased = malloc(len + 1);
    for (int i = 0; i < len; i++) {
        lowercased[i] = tolower(s[i]);
    }
    lowercased[len] = 0;
    return lowercased;
}

static void trim_slice(const char **begin, const char **end) {
    // *begin .. *end is half-open, end points one past last char
    while (*begin < *end && isspace((unsigned char)**begin)) (*begin)++;
    while (*end > *begin && isspace((unsigned char)*((*end) - 1))) (*end)--;
}

/**
 * Returns a *new* string with CH removed from STR (of length N).
 * Sets N to the new size if it is passed in. Otherwise, it will
 * compute `strlen` on STR to handle the removal.
 */
char *remove_all(const char *str, size_t *n, char ch) {
    size_t len = (n && *n) ? *n : strlen(str);

    // Allocate len + 1 so we always have room for '\0'
    char *new = malloc(len + 1);
    if (!new) {
        errno = ENOMEM;
        return NULL;
    }

    size_t new_index = 0;

    for (size_t i = 0; i < len; i++) {
        if (str[i] != ch) {
            new[new_index++] = str[i];
        }
    }

    new[new_index] = '\0';  // ALWAYS safe now

    // Resize to the actual size needed (new_index + 1 bytes)
    char *shrunk = realloc(new, new_index + 1);
    if (shrunk) new = shrunk;

    if (n) *n = new_index;

    return new;
}

static char *trim(const char *b, const char *e) {
    const ptrdiff_t len = e - b;
    char *out = (char *) malloc((int)len + 1);
    if (!out) return NULL;
    memcpy(out, b, (size_t)len);
    out[len] = '\0';
    return out;
}

char **split(const char *str, char delim, int *out_count, int *lens) {
    if (!str) return NULL;

    // First pass: count tokens
    int count = 1;  // at least one token even if no delimiter
    for (const char *p = str; *p; p++) {
        if (*p == delim) count++;
    }

    // Allocate array of pointers (+1 for NULL sentinel)
    char **result = malloc((count + 1) * sizeof(char *));
    if (!result) return NULL;

    int idx = 0;
    const char *start = str;
    const char *p = str;

    while (1) {
        if (*p == delim || *p == '\0') {
            size_t len = (size_t)(p - start);
            char *token = malloc(len + 1);
            if (!token) {
                // free everything if OOM
                for (int j = 0; j < idx; j++) free(result[j]);
                free(result);
                return NULL;
            }
            memcpy(token, start, len);
            token[len] = '\0';
            lens[idx] = len;
            result[idx++] = token;

            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }

    result[idx] = NULL; // NULL terminate like argv[]
    if (out_count) *out_count = idx;
    return result;
}

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


#include "web.h"
#include <curl/urlapi.h>

char *mk_GET(struct url_s *url) {
    if (!url) {
        return null(EINVAL);
    }
    char *request = string(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        url->pathname->bytes, url->host->bytes
    );
    if (!request) {
        return null(ENOMEM);
    }
    return request;
}

struct url_s *parse_url(const char *url) {
    struct url_s *map = url_alloc();
    if (!map) {
        return null(ENOMEM);
    }

    CURLU *u = curl_url();
    curl_url_set(u, CURLUPART_URL, url, 0);

    char *host = NULL;
    curl_url_get(u, CURLUPART_HOST, &host, 0);
    if (!host) {
        curl_url_cleanup(u);
        return null(EINVAL);
    }

    char *port = NULL;
    curl_url_get(u, CURLUPART_PORT, &port, CURLU_DEFAULT_PORT);
    if (!port) {
        if (host) {
            curl_free(host);
        }
        curl_url_cleanup(u);
        return null(EINVAL);
    }

    char *pathname = NULL;
    curl_url_get(u, CURLUPART_PATH, &pathname, 0);
    if (!pathname) {
        if (port) {curl_free(host);}
        if (host) {curl_free(host);}
        if (u) {curl_url_cleanup(u);}
        return null(EINVAL);
    }

    curl_url_cleanup(u);
    map->host = buffer_mv_string(host, 0);
    map->port = buffer_mv_string(port, 0);
    map->pathname = buffer_mv_string(pathname, 0);
    return map;
}

#endif
