#include "yarts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE *res = fetch("https://jsonplaceholder.typicode.com/todos", (const char *[]){0});
    if (!res) {
        return 1;
    }

    char *line = NULL;
    size_t cap = 0;

    while (getline(&line, &cap, res) != -1) {
        printf("%s", line);
    }

    free(line);
    fclose(res);
    return 0;
}
