#include "lib/bhop.h"
#include "lib/fetch.h"
#include <asm-generic/errno-base.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

int bhop(FILE *files[2]) {
    struct deque8 *dq = calloc(1, sizeof(struct deque8));
    if (!dq) {
        return perror_rc(-1, "calloc", deque8_free(dq));
    }
    deque8_init(dq);

    FILE *writable = bhop_writable(dq);
    if (!writable) {
        return perror_rc(-1, "bhop_writable", deque8_free(dq));
    }
    FILE *readable = bhop_readable(dq);
    if (!readable) {
        return perror_rc(-1, "bhop_readable", fclose(writable), deque8_free(dq));
    }

    files[0] = writable;
    files[1] = readable;
    return 0;
}

FILE *fetch(const char *url, const char *init[4]) {
    int fds[4] = {0};
    struct dispatch *dispatch = fetch_socket(url, init);
    if (!dispatch) {
        return perror_rc(NULL, "fetch_socket()", 0);
    }
    char *hostname = strdup(dispatch->url.hostname.hd);
    int rc = use_fetch(fds, dispatch);
    if (rc != 0) {
        perror("use_fetch()");
        return NULL;
    }
    struct fetch_state *fs = calloc(1, sizeof(struct fetch_state));
    if (!fs) {
        perror("calloc()");
        return NULL;
    }

    fs->ssl = dispatch->ssl;
    fs->ssl_ctx = dispatch->ctx;
    fs->netfd = fds[0];
    int appfd = fds[1];
    fs->outfd = fds[2];
    fs->ep = fds[3];
    fs->headers_done = false;
    fs->header_len = 0;
    fs->hostname = hostname;

    // Initialize body parsing state
    fs->chunked_mode = false;
    fs->current_chunk_size = 0;
    fs->reading_chunk_size = true;
    fs->chunk_line_len = 0;

    if (bhop(fs->bass)) {
        return perror_rc(NULL, "bhop()", close(appfd));
    }

    fs->http_done = false;

    // spawn background worker thread
    pthread_t tid;
    pthread_create(&tid, NULL, fetcher, fs);

    // detach so it cleans up after finishing
    pthread_detach(tid);

    FILE *fetchfile = fdopen(appfd, "r");
    if (!fetchfile) {
        return perror_rc(NULL, "fdopen()", close(appfd));
    }
    dispatch_free(dispatch);
    return fetchfile;
}
