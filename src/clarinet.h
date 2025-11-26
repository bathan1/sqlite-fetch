#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <yyjson.h>
#include <yajl/yajl_parse.h>

#define MAX_DEPTH 64

struct queue_s {
    char   **handle;   // array of char* this is what you need to free
    size_t   cap;     // total capacity
    size_t   head;    // next pop index
    size_t   tail;    // next push index
    size_t   count;   // number of items
};

struct clarinet_state {
    unsigned int current_depth;
    unsigned int depth;
    struct queue_s queue;

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

yajl_handle use_clarinet(clarinet_state_t *init);
void clarinet_pipe(clarinet_state_t *state, int sockfd);
void clarinet_free(clarinet_state_t *state);

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif


static void queue_init(struct queue_s *q) {
    q->cap = 8;
    q->handle = calloc(q->cap, sizeof(char *));
    q->head = q->tail = q->count = 0;
}

static void queue_free(struct queue_s *q) {
    if (!q->handle) return;

    for (size_t i = 0; i < q->count; i++) {
        size_t idx = (q->head + i) % q->cap;

        if (q->handle[idx])
            free(q->handle[idx]);
    }

    free(q->handle);
}

static void queue_push(struct queue_s *q, char *val) {
    if (q->count == q->cap) {
        size_t oldcap = q->cap;
        size_t newcap = oldcap * 2;

        char **newbuf = calloc(newcap, sizeof(char *));
        if (!newbuf) return;

        // copy linearized existing content into new buffer
        // in order (head ... oldcap-1, 0 ... head-1)
        for (size_t i = 0; i < q->count; i++) {
            size_t idx = (q->head + i) % oldcap;
            newbuf[i] = q->handle[idx];
        }

        free(q->handle);
        q->handle = newbuf;

        q->head = 0;
        q->tail = q->count;
        q->cap  = newcap;
    }

    q->handle[q->tail] = val;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
}

static char *queue_pop(struct queue_s *q) {
    if (q->count == 0)
        return NULL;

    char *val = q->handle[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;

    return val;
}
