//! [bhop basic usage]
#define _GNU_SOURCE
#include <yarts.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE *jsonfd[2] = {0};
    if (bhop(jsonfd)) {
        perror("bhop()");
        return 1;
    }

    const char input[] = "[{\"hello\":\"world\"},{\"foo\":\"bar\"}]";
    fwrite(input, 1, sizeof(input), jsonfd[0]);

    // Close write end when done.
    fclose(jsonfd[0]);

    // Now read objects back as newline-delimited JSON
    char *line = NULL;
    size_t cap = 0;

    printf("Bassoon output:\n");

    while (getline(&line, &cap, jsonfd[1]) != -1) {
        printf("%s", line);   // already includes '\n'
        free(line);
    }

    fclose(jsonfd[1]);

    return 0;
}
//! [bhop basic usage]
