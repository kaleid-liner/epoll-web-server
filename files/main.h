#include <limits.h>

#define MAX_URL 4096
#define MAX_METHOD 8
#define MAX_VERSION 16
#define MAX_HEADER 8192
// 10M
#define BUF_SIZE 10485760
#define THREAD_NUM 4
#define MAX_EVENTS 64

typedef enum STATUS {
    OK,   // OK
    ISE,  // Internal Server Error
    NF    // Not Found
} status_t;

typedef struct RequestInfo {
    int connfd; // connection file descriptor

    char uri[MAX_URL];
    char method[MAX_METHOD];
    char version[MAX_VERSION];
} request_t;

const char *content_404 = "404 Not Found";
const char *content_500 = "500 Internal Server Error";
const char *content_200 = "Hello from OSH web server";

void send_file_response(int connfd, FILE *file);
void send_response(int connfd, status_t status, const char *content, size_t content_length);
void handle_request(const request_t *req);
int parse_request(const char *req_str, request_t *req_info);

typedef struct HttpStatus {
    int connfd;
    char *header;
    size_t readn;
} http_status_t;

// 0 if not end, 1 if end response
int server(http_status_t *status);

struct thread_args {
    int listenfd;
    int epollfd;
};

void *thread(void *args);

void setnonblocking(int fd);