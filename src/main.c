#include "fetch.h"
#include "clarinet.h"
#include <sys/socket.h>

int main() {
    clarinet_t *clr = use_clarinet();
    int sockfd = fetch("http://jsonplaceholder.typicode.com/todos");
    char buf[4096] = {0};
    ssize_t n = 0;

    while ((n = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        fwrite((unsigned char *) buf, sizeof(char), n, clr->writable);
    }

    fclose(clr->writable);

    while (clr->queue->count > 0) {
        char *pop = queue_pop(clr->queue);
        free(pop);
        // printf("%s\n", pop);
    }
    queue_free(clr->queue);
    free(clr->keys);
    free(clr);

    return 0;
}
