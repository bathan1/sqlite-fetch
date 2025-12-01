#define _GNU_SOURCE
#include <yarts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE *stream = fetch("http://jsonplaceholder.typicode.com/todos", (const char *[]) {
        "GET", // method
        0, // headers
        0, // body
        FRAME_NDJSON
    });

    char *line = NULL;
    size_t cap = 0;

    while (getline(&line, &cap, stream) != -1) {
        printf("%s", line);
    }

    free(line);
    fclose(stream);

    return 0;
}
