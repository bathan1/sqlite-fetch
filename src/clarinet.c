#include "clarinet.h"

yajl_handle use_clarinet(clarinet_state_t *init) {
    queue_init(&init->queue);
    init->keys_cap = 1 << 8;
    init->keys = calloc(1 << 8, sizeof(char *));
    init->keys_size = 0;
    return yajl_alloc(&callbacks, NULL, (void *) init);
}

void clarinet_free(clarinet_state_t *state) {
    free(state->keys);
    for (int i = 0; i < state->queue.count; i++) {
        free(state->queue.handle[i]);
    }
    free(state->queue.handle);
}

#undef push
#undef peek
#undef MAX_DEPTH
