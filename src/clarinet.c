#include "clarinet.h"
#include <sys/socket.h>

#define peek(cur, field) (cur->field[cur->current_depth - 1])
#define push(cur, field, value) ((cur->field[cur->current_depth]) = value)

static int handle_null(void *ctx) {
    clarinet_state_t *state = ctx;
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
    clarinet_state_t *state = ctx;
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
    printf("here!\n");
    return 1;
}

static int handle_double(void *ctx, double d) {
    return 1;
}


static int handle_number(void *ctx, const char *num, size_t len) {
    clarinet_state_t *cur = ctx;
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
    clarinet_state_t *cur = ctx;

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
    clarinet_state_t *cur = ctx;
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
    clarinet_state_t *cur = ctx;
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
    clarinet_state_t *cur = ctx;
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
                free(cur->keys[i]);
            }
        }
        cur->keys_size = 0;

        // we push to queue
        queue_push(&cur->queue, yyjson_write(final, cur->pp_flags, NULL));

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

yajl_handle use_clarinet(struct clarinet_state *init) {
    queue_init(&init->queue);
    init->keys_cap = 1 << 8;
    init->keys = calloc(1 << 8, sizeof(char *));
    init->keys_size = 0;
    return yajl_alloc(&callbacks, NULL, (void *) init);
}

void clarinet_free(clarinet_state_t *state) {
    if (!state) {
        return;
    }
    if (state->keys) {
        // we free the key buffers but keep the parent poitner until the end
        free(state->keys);
    }
    if (state->queue.handle) {
        for (int i = 0; i < state->queue.count; i++) {
            if (state->queue.handle[i]) {
                free(state->queue.handle[i]);
            }
        }
        free(state->queue.handle);
    }
}


#undef push
#undef peek
