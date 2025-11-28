#include "fetch.h"
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t read_full(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = recv(fd, (char*)buf + off, len - off, 0);
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;               // real error
        }
        off += n;
    }
    return off;
}

char *fetch_pop(int fd, size_t *length) {
    uint64_t len = 0;
    for (;;) {
        ssize_t n = read_full(fd, &len, sizeof(len));
        if (n == 0) {
            return NULL;
        } else if (n < 0) {continue;}

        if (length) {
            *length = len;
        }
        char *obj = malloc(len + 1);
        if (!obj) {return NULL;}

        ssize_t m = read_full(fd, obj, len);
        if (m == 0) { free(obj); return NULL; }
        if (m < 0) { free(obj); continue; }
        obj[len] = 0;
        return obj;
    }
}

int main() {
    int fd = fetch("http://jsonplaceholder.typicode.com/todos", (fetch_init_t) {0});

    uint64_t len = 0;

    int count = 0;
    while (1) {
        char *obj = fetch_pop(fd, &len);
        if (!obj) {
            close(fd);
            break;
        }
        printf("%s\n", obj);
        free(obj);
        count++;
    }

    return 0;
}

