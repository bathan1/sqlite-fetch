#include <curl/curl.h>
#include <stdio.h>
#include "yarts.h"
#include <yajl/yajl_parse.h>
#include <sys/types.h>

#include <stdio.h>
#include <curl/curl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>


int main(int argc, char **argv)
{
    int sockfd = yarts_connect("https://jsonplaceholder.typicode.com/todos");
    char buf[4096];
    int n;

    while ((n = yarts_recv(sockfd, buf, sizeof(buf))) > 0) {
        printf("%d\n", n);
        fwrite(buf, 1, n, stdout);
    }
    // SSL_shutdown(ssl);
    // SSL_free(ssl);
    // SSL_CTX_free(ctx);
    close(sockfd);

    return 0;
}

