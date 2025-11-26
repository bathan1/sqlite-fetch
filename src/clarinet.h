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

static void queue_push(struct queue_s *q, char *val) {
    if (q->count == q->cap) {
        size_t newcap = q->cap * 2;
        char **newbuf = realloc(q->handle, newcap * sizeof(char *));
        if (!newbuf) return; // OOM
        q->handle = newbuf;
        q->cap   = newcap;

        // Ring-buffer correction when wrapped
        if (q->tail < q->head) {
            // move wrapped segment to the end of the new buffer
            memmove(
                &q->handle[q->cap / 2 + q->head],
                &q->handle[q->head],
                (q->cap/2 - q->head) * sizeof(char *)
            );
            q->head += q->cap / 2;
        }
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
