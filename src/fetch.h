#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/**
 * Connects to host at URL over tcp, then returns the socket fd
 * *after* sending the corresponding HTTP request encoded in URL.
 */
int fetch(const char *url);

/**
 * A 'fat' string that stores the c string first, then
 * stores its size after the null terminator '\0'.
 *
 * For example, "hi" on big endian would be encoded as:
 *
 * |----0-----|----1----|----2----|----3----| .... |----10----|
 * |    'h'   |   'i'   |    0    |    0    |      | 00000010 |
 */
typedef char buffer;

/**
 * Returns a char * buffer that is meant to hold a string of size LENGTH.
 * Uses 64 bits (or whatever your `unsigned long` size is) to save the length.
 */
static buffer *string(const char *fmt, ...) {
    va_list ap, ap2;

    // --- First pass: measure ---
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) {
        va_end(ap2);
        return NULL;
    }

    size_t len = (size_t)needed;

    // --- Allocate fat-string: bytes + '\0' + footer ---
    char *p = calloc(len + 1 + sizeof(size_t), 1);
    if (!p) {
        va_end(ap2);
        return NULL;
    }

    // --- Second pass: write formatted string ---
    vsnprintf(p, len + 1, fmt, ap2);
    va_end(ap2);

    // --- Write footer metadata ---
    size_t *meta = (size_t *)(p + len + 1);
    *meta = len;

    return p;
}

// constant time lookup
static size_t len(const buffer *p) {
    size_t *meta = (size_t *)(p + strlen(p) + 1);
    return *meta;
}
