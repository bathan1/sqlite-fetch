#include <stdio.h>
#include <stdlib.h>
#include "bassoon.h"

void bassoon_init(struct bassoon *bass) {
    bass->cap = 8;
    bass->buffer = calloc(bass->cap, sizeof(char *));
    bass->hd = bass->tl = bass->count = 0;
}

void bassoon_push(struct bassoon *q, char *val) {
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

void bassoon_free(struct bassoon *q) {
    if (!q || !q->buffer) return;

    for (size_t i = 0; i < q->count; i++) {
        size_t idx = (q->hd + i) % q->cap;

        if (q->buffer[idx])
            free(q->buffer[idx]);
    }

    free(q->buffer);
    free(q);
}

char *bassoon_pop(struct bassoon *q) {
    if (q->count == 0)
        return NULL;

    char *val = q->buffer[q->hd];
    q->buffer[q->hd] = 0;
    q->hd = (q->hd + 1) % q->cap;
    q->count--;

    return val;
}
