#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <unistd.h>
#define MAX_CONN 10

int open_listenfd(uint16_t port);
size_t rio_writen(int fd, const char *usrbuf, size_t n);