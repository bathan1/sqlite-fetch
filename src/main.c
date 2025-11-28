#include "fetch.h"
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

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

