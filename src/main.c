#include <stddef.h>
#include <stdio.h>

typedef char byte;

size_t len(byte *buf) {
    return *(size_t *)buf;
}

int main() {
    byte buf[] = {0, 0, 0, 0, 0, 0, 0, 0, 2, 'h', 'i', 0};
    printf("%s\n", buf + sizeof(size_t));
    return 0;
}
