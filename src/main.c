#include "fetch.h"
#include "clarinet.h"
#include <sys/socket.h>

int main() {
    clarinet_state_t state = {0};
    yajl_handle clarinet = use_clarinet(&state);
    int sockfd = fetch("http://jsonplaceholder.typicode.com/todos");
    char buf[4096] = {0};
    ssize_t n = 0;
    while ((n = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        yajl_parse(clarinet, (unsigned char *) buf, n);
    }
    yajl_free(clarinet);
    clarinet_free(&state);
    return 0;
}
