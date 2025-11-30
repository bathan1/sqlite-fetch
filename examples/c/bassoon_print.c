#include <yarts/bassoon.h>
#include <stdlib.h>
 
int main() {
    struct bassoon *bq = bass();
    const char hello[] = "[{\"hello\": \"world\"}, {\"foo\": \"bar\"}]";
    fwrite(hello, sizeof(char), sizeof(hello), bq->writable);
    fclose(bq->writable);

    while (bq->count > 0) {
        char *popped = bass_pop(bq);
        printf("%s\n", popped);
        free(popped);
    }
    // Prints
    // {"hello":"world"}
    // {"foo": "bar"}

    bass_free(bq);
    return 0;
}
