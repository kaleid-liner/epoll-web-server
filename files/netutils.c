#include "netutils.h"
#include <errno.h>

int open_listenfd(uint16_t port) {
    struct sockaddr_in sevr_addr;

    sevr_addr.sin_family = AF_INET;
    sevr_addr.sin_port = htons(port);
    sevr_addr.sin_addr.s_addr = INADDR_ANY;

    int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0) {
        fprintf(stderr, "Error while opening listenfd\n");
        return -1;
    }
    
    if (bind(listenfd, (struct sockaddr*)&sevr_addr, sizeof(sevr_addr)) != 0) {
        fprintf(stderr, "Error while binding listenfd to address\n");
        return -1;
    }
    if (listen(listenfd, MAX_CONN) < 0) {
        fprintf(stderr, "Error while listening on listenfd\n");
        return -1;
    }

    return listenfd;
}

size_t rio_writen(int fd, const char *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    const char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            return 0;
        }
        nleft -= nwritten;
        bufp += nwritten;
    }

    return n;
}