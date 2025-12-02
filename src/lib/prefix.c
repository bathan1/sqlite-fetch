#include "prefix.h"
#include "cfns.h"
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pre *prefix_static(const char *s, size_t len) {
    size_t total = sizeof(size_t) + len + 1;  // prefix + chars + null
    char *buf = malloc(total);
    if (!buf) {
        return perror_rc(NULL, "malloc()", 0);
    }

    /* Write prefix (binary size_t) */
    memcpy(buf, &len, sizeof(size_t));

    /* Copy raw characters */
    memcpy(buf + sizeof(size_t), s, len);

    /* Null-terminate payload */
    buf[sizeof(size_t) + len] = '\0';

    return (pre *)buf;
}

pre *prefix(const char *fmt, ...) {
    va_list ap, ap2;

    // --- First pass: measure ---
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) {
        errno = EINVAL;
        va_end(ap2);
        return NULL;
    }

    size_t len = (size_t)needed;

    // Layout:
    // [ size_t ][ chars... ][ '\0' ]
    size_t total = sizeof(size_t) + len + 1;

    char *p = (char *) calloc(1, total);
    if (!p) {
        errno = ENOMEM;
        va_end(ap2);
        return NULL;
    }

    // --- Write length header ---
    *(size_t *)p = len;

    // --- Write formatted string after header ---
    char *str = p + sizeof(size_t);
    vsnprintf(str, len + 1, fmt, ap2);
    va_end(ap2);

    return p;
}

pre *rmch(const pre *s, char ch) {
    size_t length = len(s);
    const char *raw = str(s);  // <-- correct way to access payload

    // Worst-case buffer: all characters kept + null terminator
    size_t alloc = sizeof(size_t) + length + 1;
    char *new = malloc(alloc);
    if (!new) {
        errno = ENOMEM;
        return NULL;
    }

    size_t out = sizeof(size_t);  // payload write offset

    for (size_t i = 0; i < length; i++) {
        if (raw[i] != ch) {
            new[out++] = raw[i];
        }
    }

    new[out] = '\0';

    // Calculate final payload length
    size_t payload_len = out - sizeof(size_t);

    // Write prefix correctly
    memcpy(new, &payload_len, sizeof(size_t));

    // Shrink allocation to exact size (prefix + payload + '\0')
    size_t final_size = sizeof(size_t) + payload_len + 1;
    char *shrunk = realloc(new, final_size);
    if (shrunk) new = shrunk;

    return (pre *)new;
}

pre *rmch_own(pre *s, char ch) {
    pre *next = rmch(s, ch);
    if (!next) {
        return perror_rc(NULL, "rmch()", 0);
    }
    free(s);
    return next;
}

pre **split_on_ch(const char *str, char delim, size_t *token_count) {
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
            if (p - start <= 0) {
                for (int i = 0; i < idx; i++) free(result[i]);
                free(result);
                errno = ENOMEM;
                return NULL;
            }
            char *token = prefix_static(start, p - start);
            if (!token) { // bail
                for (int i = 0; i < idx; i++) free(result[i]);
                free(result);
                errno = ENOMEM;
                return NULL;
            }
            result[idx++] = token;

            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }

    result[idx] = NULL; // NULL terminate like argv[]
    if (token_count) *token_count = idx;
    return result;
}

pre *lowercase(const pre *s) {
    size_t n = len(s);
    pre *out = malloc(sizeof(size_t) + n + 1);
    if (!out) return perror_rc(NULL, "malloc", 0);

    memcpy(out, &n, sizeof(size_t));                // length

    char *dst = (char *)out + sizeof(size_t);
    const char *src = str(s);

    for (size_t i = 0; i < n; i++)
        dst[i] = tolower((unsigned char)src[i]);

    dst[n] = '\0';

    return out;
}

pre *lowercase_own(pre *s) {
    pre *next = lowercase(s);
    if (!next) {
        return perror_rc(NULL, "lowercase", 0);
    }
    free(s);
    return next;
}
