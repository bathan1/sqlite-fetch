#include "yarts/bassoon.h"
#include <stdlib.h>
#include <string.h>
 
int main(void) {
    FILE *jsonfd[2] = {0};
    if (bhop(jsonfd)) {
        perror("bhop()");
        return 1;
    }

    // Write *raw* JSON bytes into the streaming parser
    const char input[] = "[{\"hello\":\"world\"},{\"foo\":\"bar\"}]";
    fwrite(input, 1, sizeof(input), jsonfd[0]);

    // Important: closing the writable FILE* forces parser flush + EOF
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
    // bass_free(bass);

    return 0;
}

