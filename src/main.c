#include "clarinet.h"

int main() {
    yajl_handle handle = clarinet();

    unsigned char myJson[] = 
        "{\"hello\": \"world\","
        "\"int\": 3,"
        "\"float\": 1.5,"
        "\"object_child\": {\"depth\": 1}"
        "}"
    ;
    yajl_status stat = yajl_parse(handle, myJson, sizeof(myJson));
    printf("%s\n", yajl_status_to_string(stat));
    printf("%s\n", yajl_get_error(handle, 1, myJson, sizeof(myJson)));
    return 0;
}
