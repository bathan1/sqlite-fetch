#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <yyjson.h>
#include <yajl/yajl_parse.h>

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif
#define MAX_DEPTH 64

/// An 'opaque' queue...
typedef struct clarinet {
    char   **handle;   // array of char* this is what you need to free
    size_t   cap;     // total capacity
    size_t   head;    // next pop index
    size_t   tail;    // next push index
    size_t   count;   // number of items

    FILE *writable;
} clarinet_t;

struct clarinet_state {
    unsigned int current_depth;
    unsigned int depth;
    struct clarinet *queue;

    // clarinet frees everything from here
    char **keys;
    size_t keys_size;
    size_t keys_cap;
    // key stack
    char *key[MAX_DEPTH];
    yyjson_mut_doc *doc_root;
    // object node stack
    yyjson_mut_val *object[MAX_DEPTH];

    unsigned int pp_flags;
};
typedef struct clarinet_state clarinet_state_t;

struct clarinet *use_clarinet();
void clarq_free(struct clarinet *q);
char *clarq_pop(struct clarinet *q);
