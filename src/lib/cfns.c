#include "cfns.h"
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct string dynamic(const char *fmt, ...) {
    struct string out = {0};
    va_list ap, ap2;

    // --- First pass: measure ---
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) {
        errno = EINVAL;
        va_end(ap2);
        return out;
    }

    size_t len = (size_t)needed;
    char *p = (char *) calloc(1, len + 1);
    if (!p) {
        va_end(ap2);
        return out;
    }

    vsnprintf(p, len + 1, fmt, ap2);
    va_end(ap2);
    return (struct string) {
        .hd = p,
        .length = len
    };
}

struct string slice(const struct string s, size_t start, size_t end) {
    struct string out = { .hd = NULL, .length = 0 };
    if (!s.hd || start > s.length || end > start) return out;
    struct string slice = {
        .hd=NULL, .length=0
    };
    size_t len = end - start;

    char *buf = malloc(len + 1);
    if (!buf) return out;

    memcpy(buf, s.hd + start, len);
    buf[len] = '\0';

    out.hd = buf;
    out.length = len;
    return out;
}


struct string stringdup(const struct string s) {
    return (struct string) {
        .hd = strndup(s.hd, s.length),
        .length = s.length
    };
}

struct string *split(const struct string s,
                     const struct string pattern,
                     size_t *ntokens)
{
    if (!s.hd || !pattern.hd || !ntokens) {
        return NULL;
    };
    *ntokens = 0;

    const char *src = s.hd;
    size_t slen = s.length;

    const char *pat = pattern.hd;
    size_t plen = pattern.length;

    // Edge case: empty pattern -> return whole string as 1 token
    if (plen == 0) {
        struct string *arr = calloc(1, sizeof(*arr));
        if (!arr) return NULL;
        arr[0] = stringdup(s);
        *ntokens = 1;
        return arr;
    }

    size_t count = 1;
    for (size_t i = 0; i + plen <= slen; ) {
        if (memcmp(src + i, pat, plen) == 0) {
            count++;
            i += plen;
        } else {
            i++;
        }
    }

    // token array
    struct string *parts = calloc(count, sizeof(*parts));
    if (!parts) return NULL;

    size_t idx = 0;
    size_t start = 0;

    for (size_t i = 0; i <= slen; ) {

        bool at_end = (i == slen);
        bool at_pat = false;

        if (!at_end && i + plen <= slen) {
            at_pat = (memcmp(src + i, pat, plen) == 0);
        }

        if (at_pat || at_end) {

            // token = substring [start, i)
            struct string token = slice(s, start, i);

            if (!token.hd) {
                // cleanup all previous tokens
                for (size_t k = 0; k < idx; k++)
                    free(parts[k].hd);
                free(parts);
                *ntokens = 0;
                return NULL;
            }

            parts[idx++] = token;

            if (at_pat) {
                i += plen;
                start = i;
                continue;
            }

            if (at_end) break;
        }

        i++;
    }

    *ntokens = count;
    return parts;
}

struct string *splitch(const struct string s, char delim,
                       size_t *ntoks)
{
    if (!s.hd || !ntoks) {
        errno = EINVAL;
        return NULL;
    }

    size_t count = 1;
    for (const char *p = s.hd; *p; p++) {
        if (*p == delim) count++;
    }

    struct string *tokens = calloc(count, sizeof(struct string));
    if (!tokens) return NULL;

    int index = 0;
    const char *start = s.hd;
    const char *p = s.hd;

    for (;;) {
        if (*p == delim || *p == '\0') {
            size_t token_length = p - start;
            if (token_length <= 0) {
                for (int i = 0; i < index; i++) {
                    free(tokens[i].hd);
                }
                free(tokens);
                errno = EINVAL;
                return NULL;
            }
            char *token_buffer = calloc(token_length + 1, sizeof(char));
            if (!token_buffer) { // bail
                for (size_t i = 0; i < index; i++) free(tokens[i].hd);
                free(tokens);
                return NULL;
            }
            memcpy(token_buffer, start, token_length);
            token_buffer[token_length] = '\0';

            tokens[index].hd = token_buffer;
            tokens[index].length = token_length;
            index++;

            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    *ntoks = index;
    return tokens;
}

int lowercase(struct string *s) {
    if (!s || !s->hd) return 1;
    for (int i = 0; i < s->length; i++) {
        s->hd[i] = tolower(s->hd[i]);
    }
    return 0;
}

int rmch(struct string *s, char ch) {
    if (!s || !s->hd) return 1;
    int w = 0;
    for (int r = 0; r < s->length; r++) {
        if (s->hd[r] != ch) {
            s->hd[w++] = s->hd[r];
        }
    }
    s->hd[w] = '\0';
    s->length = w;
    return 0;
}

struct string lowercase_im(const struct string s) {
    struct string out = {0};
    if (!s.hd) return out;

    out.length = s.length;
    out.hd = calloc(out.length + 1, sizeof(char));
    if (!out.hd) {
        return out;
    }
    memcpy(out.hd, s.hd, out.length + 1);
    if (lowercase(&out) != 0) {
        free(out.hd);
        return out;
    }
    return out;
}
