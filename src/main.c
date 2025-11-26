#include "clarinet.h"

int main() {
    clarinet_state_t ctx = {0};
    yajl_handle handle = use_clarinet(&ctx);

    unsigned char myJson[] = 
        "["
            "{\"hello\": \"world\","
            "\"int\": 3,"
            "\"float\": 1.5,"
            "\"object\": {\"depth\": 1}"
            "},"
            "{\"foo\": \"bar\","
            "\"int\": 3,"
            "\"float\": 1.5,"
            "\"object\": {\"depth\": 1}"
            "}"
        "]"
    ;
    yajl_status stat = yajl_parse(handle, myJson, sizeof(myJson) - 1);
    printf("%s\n", yajl_status_to_string(stat));
    yajl_free(handle);

    printf("%zu objects in queue\n", ctx.queue.count);
    for (int i = 0; i < ctx.queue.count; i++) {
        printf("%s\n", ctx.queue.handle[i]);
    }

    free_clarinet(&ctx);
    return 0;
}
