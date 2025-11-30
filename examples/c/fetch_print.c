#define _GNU_SOURCE
#include <yarts/fetch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
int main(void) {
    FILE *stream = fetch("http://jsonplaceholder.typicode.com/todos", (fetch_init_t) {
        .method = "GET",
        .frame = FRAME_JSON_OBJ,
        .headers = 0,
        .body = 0,
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

