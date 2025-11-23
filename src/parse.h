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
    size_t len = (n != NULL) ? *n : 0;
    if (len < 1) {
        len = strlen(str);
    }

    char *new = (char *) malloc(len);
    if (!new) {
        errno = ENOMEM;
        return NULL;
    }
    int new_index = 0;

    for (int i = 0; i < len; i++) {
        char curr_ch = str[i];
        if (curr_ch != ch) {
            new[new_index++] = curr_ch;
        }
    }
    // new_index is length if there are no matches on CH.
    if (new_index != len) {
        new[new_index] = '\0';
        new = realloc(new, new_index);
        if (n) {
            *n = new_index;
        }
    }
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

#endif
