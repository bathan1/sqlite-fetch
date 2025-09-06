/* strmap.h — tiny case-insensitive string -> pointer hashmap (public domain-ish) */
#ifndef STRMAP_H
#define STRMAP_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --- utils --- */
static inline char *sm_xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    return p ? (char *)memcpy(p, s, n) : NULL;
}
static inline int sm_ieq(const char *a, const char *b) { /* ASCII case-insensitive equality */
    for (;; a++, b++) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        if (!ca) return 1;
    }
}
static inline uint64_t sm_hash_ci(const char *s) {       /* FNV-1a over lowercase ASCII */
    const uint64_t FNV_OFFSET = 1469598103934665603ULL;
    const uint64_t FNV_PRIME  = 1099511628211ULL;
    uint64_t h = FNV_OFFSET;
    for (unsigned char c; (c = (unsigned char)*s++); ) {
        if (c >= 'A' && c <= 'Z') c += 32;
        h ^= c;
        h *= FNV_PRIME;
    }
    return h;
}

/* --- map --- */
typedef struct {
    uint64_t hash;
    char    *key;   /* owned strdup of key (header name) */
    void    *val;   /* your payload (e.g., buffer_t*) */
    unsigned char state; /* 0=empty, 1=full, 2=tombstone */
} sm_bucket_t;

typedef struct {
    sm_bucket_t *b;
    size_t cap;   /* always power of two */
    size_t len;   /* number of FULL slots */
} strmap_t;

static inline void sm_init(strmap_t *m) { m->b=NULL; m->cap=0; m->len=0; }

static void sm_free(strmap_t *m, int free_keys) {
    if (!m || !m->b) return;
    if (free_keys) {
        for (size_t i = 0; i < m->cap; ++i)
            if (m->b[i].state == 1) free(m->b[i].key);
    }
    free(m->b);
    m->b = NULL; m->cap = m->len = 0;
}

static int sm_rehash(strmap_t *m, size_t new_cap) {
    sm_bucket_t *old = m->b;
    size_t old_cap = m->cap;

    sm_bucket_t *nb = (sm_bucket_t *)calloc(new_cap, sizeof(sm_bucket_t));
    if (!nb) return -1;

    m->b = nb; m->cap = new_cap; m->len = 0;

    if (!old) return 0;

    for (size_t i = 0; i < old_cap; ++i) if (old[i].state == 1) {
        uint64_t h = old[i].hash;
        size_t mask = new_cap - 1;
        size_t j = (size_t)h & mask;
        while (nb[j].state == 1) j = (j + 1) & mask;
        nb[j].state = 1;
        nb[j].hash  = h;
        nb[j].key   = old[i].key; /* transfer ownership */
        nb[j].val   = old[i].val;
        m->len++;
    }
    free(old);
    return 0;
}

static inline size_t sm_next_pow2(size_t x) {
    if (x < 8) return 8;
    --x; x |= x>>1; x |= x>>2; x |= x>>4; x |= x>>8; x |= x>>16;
#if SIZE_MAX > 0xFFFFFFFFu
    x |= x>>32;
#endif
    return x + 1;
}

static int sm_maybe_grow(strmap_t *m) {
    if (m->cap == 0) return sm_rehash(m, 8);
    /* load factor ~0.75 */
    if ((m->len + 1) * 4 <= m->cap * 3) return 0;
    return sm_rehash(m, m->cap * 2);
}

/* insert or replace; returns 0 on success, -1 on OOM */
static int sm_set(strmap_t *m, const char *key, void *val) {
    if (sm_maybe_grow(m)) return -1;

    uint64_t h = sm_hash_ci(key);
    size_t mask = m->cap - 1;
    size_t i = (size_t)h & mask;
    size_t tomb = (size_t)-1;

    for (;;) {
        sm_bucket_t *bk = &m->b[i];
        if (bk->state == 0) { /* empty */
            size_t slot = (tomb != (size_t)-1) ? tomb : i;
            sm_bucket_t *t = &m->b[slot];
            t->state = 1; t->hash = h;
            t->key = sm_xstrdup(key);
            if (!t->key) return -1;
            t->val = val;
            m->len++;
            return 0;
        }
        if (bk->state == 2) { /* tombstone */
            if (tomb == (size_t)-1) tomb = i;
        } else if (bk->hash == h && sm_ieq(bk->key, key)) {
            /* replace */
            bk->val = val;
            return 0;
        }
        i = (i + 1) & mask;
    }
}

/* returns NULL if not found */
static void *sm_get(const strmap_t *m, const char *key) {
    if (!m || !m->b) return NULL;
    uint64_t h = sm_hash_ci(key);
    size_t mask = m->cap - 1;
    size_t i = (size_t)h & mask;

    for (;;) {
        const sm_bucket_t *bk = &m->b[i];
        if (bk->state == 0) return NULL; /* empty: stop */
        if (bk->state == 1 && bk->hash == h && sm_ieq(bk->key, key))
            return bk->val;
        i = (i + 1) & mask;
    }
}

/* delete; returns 1 if removed, 0 if not present */
static int sm_del(strmap_t *m, const char *key, int free_key) {
    if (!m || !m->b) return 0;
    uint64_t h = sm_hash_ci(key);
    size_t mask = m->cap - 1;
    size_t i = (size_t)h & mask;

    for (;;) {
        sm_bucket_t *bk = &m->b[i];
        if (bk->state == 0) return 0;
        if (bk->state == 1 && bk->hash == h && sm_ieq(bk->key, key)) {
            bk->state = 2; /* tombstone */
            if (free_key) free(bk->key);
            bk->key = NULL;
            bk->val = NULL;
            return 1;
        }
        i = (i + 1) & mask;
    }
}

#endif /* STRMAP_H */