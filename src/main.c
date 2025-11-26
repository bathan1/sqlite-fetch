#include "fetch.h"
#include "clarinet.h"
#include <sys/socket.h>

int main() {
    clarinet_t *clr = use_clarinet();
    int sockfd = fetch("http://jsonplaceholder.typicode.com/todos");
    char buf[4096] = {0};
    ssize_t n = 0;
    while ((n = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        yajl_parse((yajl_handle) clr->writable, (unsigned char *) buf, n);
    }
    clarinet_free(clr);
    return 0;
}
