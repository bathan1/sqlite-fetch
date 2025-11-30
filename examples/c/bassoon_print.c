#include <yarts/bassoon.h>
#include <stdlib.h>
 
int main() {
    struct bassoon *bass = Bassoon();
    const char hello[] = "[{\"hello\": \"world\"}, {\"foo\": \"bar\"}]";
    fwrite(hello, sizeof(char), sizeof(hello), bass->writable);
    fclose(bass->writable);

    while (bass->count > 0) {
        char *popped = bass_pop(bass);
        printf("%s\n", popped);
        free(popped);
    }
    // Prints
    // {"hello":"world"}
    // {"foo": "bar"}

    bass_free(bass);
    return 0;
}
