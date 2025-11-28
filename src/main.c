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

int main() {
    int fd = fetch("http://jsonplaceholder.typicode.com/todos");

    uint64_t len;

    int count = 0;
    while (1) {
        ssize_t n = read_full(fd, &len, sizeof(len));

        if (n == 0) break;   // EOF → done
        if (n < 0) {
            // no data yet → try again
            // but do NOT keep calling malloc on uninitialized len!
            continue;
        }

        char *obj = malloc(len + 1);
        if (!obj) break;

        ssize_t m = read_full(fd, obj, len);
        if (m == 0) { free(obj); break; }
        if (m < 0) { free(obj); continue; }

        obj[len] = 0;
        free(obj);
        count++;
    }

    printf("main read %d total items\n", count);

    close(fd);
    return 0;
}

