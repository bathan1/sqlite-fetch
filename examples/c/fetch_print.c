//! [fetch basic usage]
#define _GNU_SOURCE
#include <yapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE *stream = fetch("https://jsonplaceholder.typicode.com/todos", 0);

    char *line = NULL;
    size_t cap = 0;

    while (getline(&line, &cap, stream) != -1) {
        printf("%s", line);
    }

    free(line);
    fclose(stream);

    return 0;
}
//! [fetch basic usage]
