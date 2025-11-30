#include "bassoon.h"
#include <stdlib.h>
#include <string.h>
 
int main(void) {
    struct bassoon *bass = Bassoon();

    // Write *raw* JSON bytes into the streaming parser
    const char input[] = "[{\"hello\":\"world\"},{\"foo\":\"bar\"}]";
    fwrite(input, 1, sizeof(input), bass->writable);

    // Important: closing the writable FILE* forces parser flush + EOF
    fclose(bass->writable);
    // Now read objects back as newline-delimited JSON
    char *line = NULL;
    size_t cap = 0;
    
    printf("Bassoon output:\n");

    while (getline(&line, &cap, bass->readable) != -1) {
        printf("%s", line);   // already includes '\n'
        free(line);
    }

    fclose(bass->readable);
    // bass_free(bass);

    return 0;
}

