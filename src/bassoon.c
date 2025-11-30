#define _GNU_SOURCE
#include <yyjson.h>
#include <yajl/yajl_parse.h>
#include "common.h"
#include "bassoon.h"


#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

/** Fixed number of JSON object levels to traverse before returning. */
#define MAX_DEPTH 64

#define peek(cur, field) (cur->field[cur->current_depth - 1])
#define push(cur, field, value) ((cur->field[cur->current_depth]) = value)

struct bassoon_state {
    yajl_handle parser;
    unsigned int current_depth;
    unsigned int depth;
    struct bassoon *queue;

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

static void queue_init(struct bassoon *q) {
    q->cap = 8;
    q->buffer = calloc(q->cap, sizeof(char *));
    q->hd = q->tl = q->count = 0;
}

static void queue_push(struct bassoon *q, char *val) {
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

static int handle_null(void *ctx) {
    struct bassoon_state *state = ctx;
    if (state->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!peek(state, key)) {
        fprintf(stderr, "no parent key value from depth %u\n", state->current_depth);
        return 0;
    }
    yyjson_mut_obj_add_null(state->doc_root, peek(state, object), peek(state, key));
    return 1;
}

static int handle_bool(void *ctx, int b) {
    struct bassoon_state *state = ctx;
    if (state->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!peek(state, key)) {
        fprintf(stderr, "no parent key value from depth %u\n", state->current_depth);
        return 0;
    }
    yyjson_mut_obj_add_bool(
        state->doc_root,
        peek(state, object),
        peek(state, key),
        b
    );
    return 1;
}

static int handle_int(void *ctx, long long i) {
    return 1;
}

static int handle_double(void *ctx, double d) {
    return 1;
}

static int handle_number(void *ctx, const char *num, size_t len) {
    struct bassoon_state *cur = ctx;
    if (cur->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!peek(cur, key)) {
        fprintf(stderr, "no parent key value from depth %u\n", cur->current_depth);
        return 0;
    }

    bool is_float = false;
    for (size_t i = 0; i < len; i++) {
        char c = num[i];
        if (c == '.' || c == 'e' || c == 'E') {
            is_float = true;
            break;
        }
    }

    if (is_float) {
        double d = strtod(num, NULL);
        yyjson_mut_obj_add_double(
            cur->doc_root,
            peek(cur, object),
            peek(cur, key),
            d
        );
    } else {
        long i = strtoll(num, NULL, 10);
        yyjson_mut_obj_add_int(
            cur->doc_root,
            peek(cur, object),
            peek(cur, key),
            i
        );
    }

    return 1;
}

static int handle_string(void *ctx,
                         const unsigned char *str, size_t len)
{
    struct bassoon_state *cur = ctx;

    if (cur->current_depth == 0 || !peek(cur, key) || !peek(cur, object)) {
        return 0;
    }

    yyjson_mut_obj_add_strncpy(
        cur->doc_root,
        peek(cur, object),
        peek(cur, key),
        (char *) str,
        len
    );

    return 1;
}

static int handle_start_map(void *ctx) {
    struct bassoon_state *cur = ctx;
    if (cur->current_depth == 0) {
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *obj = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, obj);
        cur->doc_root = doc;
        cur->object[0] = yyjson_mut_doc_get_root(doc);
    } else {
        yyjson_mut_val *new_obj = yyjson_mut_obj(cur->doc_root);
        yyjson_mut_obj_add_val(
            cur->doc_root,
            peek(cur, object),
            peek(cur, key),
            new_obj
        );
        cur->object[cur->current_depth] = new_obj;
    }

    cur->current_depth++;

    return 1;
}


static int handle_map_key(void *ctx,
                          const unsigned char *str,
                          size_t len)
{
    struct bassoon_state *cur = ctx;
    if (cur->keys_size >= cur->keys_cap) {
        // double
        cur->keys_cap *= 2;
        cur->keys = realloc(cur->keys, cur->keys_cap * sizeof(char *));
    }

    char *next_key = strndup((const char *) str, len);
    cur->keys[cur->keys_size++] = next_key;
    // Store the new key for this depth
    peek(cur, key) = next_key;

    return 1;
}

static int handle_end_map(void *ctx) {
    struct bassoon_state *cur = ctx;
    if (cur->current_depth == 1) {
        // closing root object because root object set depth to 1,
        // so that any nested object child can recursively push its own
        // node to the key / parent stack

        yyjson_doc *final =
            yyjson_mut_doc_imut_copy(cur->doc_root, NULL);
        if (!final) {
            fprintf(stderr, "could not copy to immutable doc\n");
            return 0;
        }
        yyjson_mut_doc_free(cur->doc_root);

        if (cur->keys) {
            for (int i = 0; i < cur->keys_size; i++) {
                if (cur->keys[i])
                    free(cur->keys[i]);
            }
        }
        cur->keys_size = 0;

        // we push to queue
        queue_push(cur->queue, yyjson_write(final, cur->pp_flags, NULL));

        // free(cur->queue.handle);
        yyjson_doc_free(final);
    }
    cur->current_depth--;
    cur->depth = MAX(cur->current_depth, cur->depth);

    return 1;
}

static int handle_start_array(void *ctx) {
    return 1;
}

static int handle_end_array(void *ctx) {
    return 1;
}

static yajl_callbacks callbacks = {
    .yajl_null        = handle_null,
    .yajl_boolean     = handle_bool,
    .yajl_integer     = handle_int,
    .yajl_double      = handle_double,
    .yajl_number      = handle_number,
    .yajl_string      = handle_string,

    .yajl_start_map   = handle_start_map,
    .yajl_map_key     = handle_map_key,
    .yajl_end_map     = handle_end_map,

    .yajl_start_array = handle_start_array,
    .yajl_end_array   = handle_end_array
};

static ssize_t bhopcookie_read(void *cookie, char *buf, size_t size) {
    struct bassoon *c = cookie;

    char *json = bass_pop(c);
    if (!json)
        return 0;

    size_t json_len = strlen(json);
    size_t out_len;

    /* We want to include '\n' if possible */
    if (json_len + 1 <= size) {
        /* Full JSON plus newline fits */
        memcpy(buf, json, json_len);
        buf[json_len] = '\n';
        out_len = json_len + 1;
    } else {
        /* Otherwise, we just emit truncated JSON only */
        memcpy(buf, json, size);
        out_len = size;
    }

    free(json);
    return out_len;
}

static ssize_t bhopcookie_write(void *cookie, const char *buf, size_t size) {
    struct bassoon_state *c = cookie;
    yajl_parse(c->parser, (const unsigned char *)buf, size);
    return size;
}

static int free_writable(void *cookie) {
    struct bassoon_state *cc = (void *) cookie;
    if (!cc) {
        return 1;
    }

    if (cc->parser) {
        yajl_free(cc->parser);
    }
    if (cc->keys) {
        free(cc->keys);
    }
    free(cc);

    return 0;
}

static int free_readable(void *cookie) {
    struct bassoon *queue = (void *) cookie;
    if (!queue) { return 1; }
    bass_free(queue);
    return 0;
}

static FILE *open_writable(struct bassoon_state *init) {
    yajl_handle parser = yajl_alloc(&callbacks, NULL, (void *) init);
    if (!parser) {
        bass_free(init->queue);
        free(init->keys);
        free(init);
        return NULL;
    }

    init->parser = parser;

    cookie_io_functions_t io = {
        .write = bhopcookie_write,
        .close = free_writable,
        .read  = NULL,
        .seek  = NULL,
    };

    return fopencookie(init, "w", io);
}

static FILE *open_readable(struct bassoon *init) {
    printf("%p\n", init);
    cookie_io_functions_t io = {
        .read  = bhopcookie_read,
        .close = free_readable,
        .write = NULL,
        .seek  = NULL,
    };

    return fopencookie(init, "r", io);
}

void bass_free(struct bassoon *q) {
    if (!q || !q->buffer) return;

    for (size_t i = 0; i < q->count; i++) {
        size_t idx = (q->hd + i) % q->cap;

        if (q->buffer[idx])
            free(q->buffer[idx]);
    }

    free(q->buffer);
    free(q);
}

char *bass_pop(struct bassoon *q) {
    if (q->count == 0)
        return NULL;

    char *val = q->buffer[q->hd];
    q->buffer[q->hd] = 0;
    q->hd = (q->hd + 1) % q->cap;
    q->count--;

    return val;
}

struct bassoon *Bassoon() {
    struct bassoon_state *init = calloc(1, sizeof(struct bassoon_state));
    if (!init) return null(ENOMEM);
    init->queue = calloc(1, sizeof(struct bassoon));
    if (!init->queue) return null(ENOMEM);
    queue_init(init->queue);
    init->keys_cap = 1 << 8;
    init->keys = calloc(1 << 8, sizeof(char *));
    init->keys_size = 0;

    FILE *writable = open_writable(init);
    if (!writable) {
        perror("open_writable");
        bass_free(init->queue);
        free(init->keys);
        free(init);
        return NULL;
    }
    FILE *readable = open_readable(init->queue);
    if (!readable) {
        perror("open_readable");
        bass_free(init->queue);
        free(init->keys);
        free(init);
    }
    init->queue->writable = writable;
    init->queue->readable = readable;

    return init->queue;
}

#undef push
#undef peek
#undef MAX_DEPTH
#undef MAX
