/**
 * @file fetch.h
 * @brief Simplified [Web Fetch](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API) implementation for C.
 *
 * @example examples/c/fetch_print.c
 * Demonstrates how to stream JSON with fetch.
 *
 * Compile with:
 * ```bash
 * gcc fetch_print.c -lyarts
 * ```
 *
 * See the [README](README.md) on installing onto the usr lib.
 */

#pragma once
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* FETCH FRAME OPTIONS. These are just plain integers
 * that are statically cast to `const char *` for convenience.
 * You can pass in plain integers to the frame slot and that will 
 * work just fine, you will just get some compiler warnings. */

/** 
 * @brief Parse JSON objects into NDJSON.
 *
 * This is also the default parse strategy.
 */
static const char *FRAME_NDJSON = 0;

/**
 * @brief \c send() HTTP Request over a TCP socket, wrapping the response socket over
 * the returned `FILE *` stream.
 *
 * Connects to host at URL over tcp and writes optional HTTP fields in INIT 
 * to the request. It returns a readable FILE stream that separates the frames 
 * by newlines, so you can read each logical frame one by one easier.
 *
 * INIT slots are:
 *  - [0]: Method case insensitive
 *  - [1]: Headers
 *  - [2]: Body
 *  - [3]: *plain int* Body parser frame type.
 *
 * INIT[3] is the only slot that #fetch will read as a plain
 * `uint64`.
 *
 * @retval ~0 OK - Anything not 0 means the response stream was successfully opened.
 * @retval NULL Error - Check `errno` to learn about the error (too many to list here).
 */
FILE *fetch(const char *url, const char *init[4]);

static char *append(char **bufptr, size_t len) {
    char *buf = *bufptr;

    char *p = (char *) realloc(buf, len + 1 + sizeof(size_t));
    if (!p) return NULL;

    size_t *meta = (size_t *)(p + len + 1);
    *meta = len;

    *bufptr = p;
    return p;
}

static char *to_lower(char *s, size_t len) {
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
static char *remove_all(const char *str, size_t *n, char ch) {
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

static char **split(const char *str, char delim, int *out_count, int *lens) {
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
