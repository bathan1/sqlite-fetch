#pragma once
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    STREAM_JSON_ARR,
    STREAM_TEXT
} fetch_stream_type_e;
struct fetch_init {
    char *headers;
    char *body;
    int stream_type;
};
typedef struct fetch_init fetch_init_t;
/**
 * Connects to host at URL over tcp, then returns the socket fd
 * *after* sending the corresponding HTTP request encoded in URL.
 */
int fetch(const char *url, struct fetch_init init);

/**
 * Pop the next parser framed object from the fetch buffer at FD. If LENGTH is passed in, 
 * this sets LENGTH to the length of the bytes *not including NULL TERMINATOR*, 
 * meaning the returned char buffer has total size `LENGTH + 1`.
 *
 * Returns NULL either on EOF.
 */
char *fetch_pop(int fd, size_t *length);

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

static buffer *append(char **bufptr, size_t len) {
    char *buf = *bufptr;

    char *p = realloc(buf, len + 1 + sizeof(size_t));
    if (!p) return NULL;

    size_t *meta = (size_t *)(p + len + 1);
    *meta = len;

    *bufptr = p;
    return p;
}

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

