#include "fetch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("incorrect usage: ./main https://example.com\n");
        return 1;
    }
    int sockfd = fetch(argv[1]);
    char buf[4096];
    ssize_t n;

    while ((n = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, n, stdout);
    }

    if (n < 0) perror("recv");

    close(sockfd);
    return 0;
}

