/** @file common.h YARTS utilities.
 *  @brief General convenience functions for the other modules
 */
#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

/** Sets {@link errno} to ERROR_CODE so you don't have to every time. */
static void *null(int error_code) {
    errno = error_code;
    return NULL;
}

static long long zero (int error_code) {
    return (long long) null(error_code);
}

static long long neg1 (int error_code) {
    return zero(error_code) - 1;
}

static int one (int error_code) {
    errno = error_code;
    return 1;
}

/**
 * Flattened struct type that writes `sizeof(size_t)` SIZE bytes
 * at the head, and then has SIZE + 1 bytes after for the string + nul terminator.
 *
 * For example, "hi" on big endian on most 64 bit machines would be encoded as:
 *
 * |----0-----|----1----| ... |----7----|----8----|----9----|----10----|
 * |    0     |    0    | ... |    2    |   'h'   |   'i'   |    0     |
 */
typedef char str;

/** Constant time size lookup that does the type casting for you. */
static size_t len(str *buf) {
    return *(size_t *)buf;
}

/** Like \c sprintf() except that it *returns* the buffer rather than making you pass it in. */
static str *string(const char *fmt, ...) {
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

    // Layout:
    // [ size_t ][ chars... ][ '\0' ]
    size_t total = sizeof(size_t) + len + 1;

    char *p = (char *) calloc(1, total);
    if (!p) {
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

/** 
 * Labeled pointer offset addition of \c sizeof(size_t) to S
 * so you don't need to remember to add it each time.
 */
#define stroff(s) (s + sizeof(size_t))
