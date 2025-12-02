#include <stdlib.h>
#define _GNU_SOURCE
#include <yyjson.h>
#include <yajl/yajl_parse.h>

#include "deque.h"
#include "cfns.h"

/** Fixed number of JSON object levels to traverse before returning. */
#define MAX_DEPTH 64

#define peek(cur, field) (cur->field[cur->current_depth - 1])
#define push(cur, field, value) ((cur->field[cur->current_depth]) = value)

struct bassoon_state {
    yajl_handle parser;
    unsigned int current_depth;
    unsigned int depth;
    struct deque8 *queue;

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

/** YAJL parser callbacks */
static yajl_callbacks callbacks;
/** Close write end @ COOKIE. */
static ssize_t bhop_fwrite(void *cookie, const char *buf, size_t size);
/** Close read end @ COOKIE. */
static ssize_t bhop_fread(void *cookie, char *buf, size_t size);
static int bhop_fclosew(void *cookie);
static int bhop_fcloser(void *cookie);

/** Initialize bassoon state on the heap and get back that pointer. */
static struct bassoon_state *use_state();
static void free_state(struct bassoon_state *w_bassoon);

FILE *bhop_readable(struct deque8 *init) {
    cookie_io_functions_t io = {
        .read  = bhop_fread,
        .close = bhop_fcloser,
        .write = NULL,
        .seek  = NULL,
    };

    return fopencookie(init, "r", io);
}

FILE *bhop_writable(struct deque8 *init) {
    struct bassoon_state *w_bassoon = use_state();
    if (!w_bassoon) {
        perror("use_state");
        return NULL;
    }
    w_bassoon->queue = init;
    w_bassoon->parser = yajl_alloc(&callbacks, NULL, (void *) w_bassoon);
    if (!w_bassoon->parser) {
        perror("yajl_alloc");
        free(w_bassoon);
        return NULL;
    }

    cookie_io_functions_t io = {
        .write = bhop_fwrite,
        .close = bhop_fclosew,
        .read  = NULL,
        .seek  = NULL,
    };

    return fopencookie(w_bassoon, "w", io);
}

/* BEGIN STATIC */
static ssize_t bhop_fwrite(void *cookie, const char *buf, size_t size) {
    struct bassoon_state *c = cookie;
    yajl_parse(c->parser, (const unsigned char *)buf, size);
    return size;
}

static int bhop_fclosew(void *cookie) {
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

static ssize_t bhop_fread(void *cookie, char *buf, size_t size) {
    struct deque8 *c = cookie;
    char *json = deque8_pop(c);
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

static int bhop_fcloser(void *cookie) {
    struct deque8 *queue = (void *) cookie;
    if (!queue) { return 1; }
    deque8_free(queue);
    return 0;
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

        char *json = yyjson_write(final, cur->pp_flags, NULL);
        // we push to queue
        deque8_push(cur->queue, json);

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

static struct bassoon_state *use_state(void) {
    struct bassoon_state *st = calloc(1, sizeof(struct bassoon_state));
    if (!st) return perror_rc(NULL, "calloc()", 0);

    st->keys_cap = 1 << 8;     // 256
    st->keys = calloc(st->keys_cap, sizeof(char *));
    if (!st->keys) {
        return perror_rc(NULL, "calloc()", free(st));
    }

    // IMPORTANT: do NOT allocate st->queue here.
    // It must be set by bhop_writable() to point to caller's queue.
    st->queue = NULL;

    return st;
}

static void free_state(struct bassoon_state *st) {
    if (!st) return;

    // Don't free st->queue here — it's not owned by the state!

    if (st->keys)
        free(st->keys);

    free(st);
}

#undef push
#undef peek
#undef MAX_DEPTH
