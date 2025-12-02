#include "deque.h"
#include <stdio.h>
#include <stdlib.h>

void deque8_init(struct deque8 *deque) {
    deque->cap = 8;
    deque->buffer = calloc(deque->cap, sizeof(char *));
    deque->hd = deque->tl = deque->count = 0;
}

void deque8_push(struct deque8 *q, char *val) {
    if (q->count == q->cap) {
        size_t oldcap = q->cap;
        size_t newcap = oldcap * 2;

        char **newbuf = calloc(newcap, sizeof(char *));
        if (!newbuf) return;

        // copy linearized existing content into new buffer
        // in order (head ... oldcap-1, 0 ... head-1)
        for (size_t i = 0; i < q->count; i++) {
            size_t idx = (q->hd + i) % oldcap;
            newbuf[i] = q->buffer[idx];
        }

        free(q->buffer);
        q->buffer = newbuf;

        q->hd = 0;
        q->tl = q->count;
        q->cap  = newcap;
    }

    q->buffer[q->tl] = val;
    q->tl = (q->tl + 1) % q->cap;
    q->count++;
}

void deque8_free(struct deque8 *q) {
    if (!q || !q->buffer) return;

    for (size_t i = 0; i < q->count; i++) {
        size_t idx = (q->hd + i) % q->cap;

        if (q->buffer[idx])
            free(q->buffer[idx]);
    }

    free(q->buffer);
    free(q);
}

char *deque8_pop(struct deque8 *q) {
    if (q->count == 0)
        return NULL;

    char *val = q->buffer[q->hd];
    q->buffer[q->hd] = 0;
    q->hd = (q->hd + 1) % q->cap;
    q->count--;

    return val;
}
