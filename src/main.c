#include "clarinet.h"

int main() {
    clarinet_ctx_t ctx = {0};
    yajl_handle handle = clarinet(&ctx);

    unsigned char myJson[] = 
        "{\"hello\": \"world\","
        "\"int\": 3,"
        "\"float\": 1.5,"
        "\"object\": {\"depth\": 1}"
        "}"
    ;
    yajl_status stat = yajl_parse(handle, myJson, sizeof(myJson) - 1);
    printf("%s\n", yajl_status_to_string(stat));
    yajl_free(handle);
    return 0;
}
