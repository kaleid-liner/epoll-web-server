#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include "netutils.h"
#include "main.h"

#define PORT 8000

#define min(a, b) ((a) < (b) ? (a) : (b))

int parse_request(const char *req_str, request_t *req_info) {
    if (sscanf(req_str, "%s %s %[^\r\n]", req_info->method, req_info->uri, req_info->version) != 3) {
        fprintf(stderr, "malformed http request\n");
        return -1;
    }
}

void handle_request(const request_t *req) {
    if (strncmp(req->method, "GET", 3) != 0) {
        fprintf(stderr, "only support `GET` method\n");
        return;
    }

    char *abs_path = (char *)malloc(PATH_MAX);
    if (abs_path == NULL) {
        perror("malloc");
    }
    const char *rel_path = req->uri[0] == '/' ? req->uri + 1 : req->uri;
    char *cur_dir = (char *)malloc(PATH_MAX);
    if (cur_dir == NULL) {
        perror("malloc");
    }

    realpath(rel_path, abs_path);
    getcwd(cur_dir, PATH_MAX);

    if (strncmp(cur_dir, abs_path, strlen(cur_dir)) != 0) {
        send_response(req->connfd, ISE, content_500, strlen(content_500));
        return;
    } 

    FILE *file = fopen(abs_path, "rb");
    free(abs_path);
    free(cur_dir);
    if (file == NULL) {
        if (errno == ENOENT) {
            send_response(req->connfd, NF, content_404, strlen(content_404));
        } else {
            send_response(req->connfd, ISE, content_500, strlen(content_500));
        }
        return;
    }

    send_file_response(req->connfd, file);
    fclose(file);
}

void send_response(int connfd, status_t status, 
                   const char *content, size_t content_length) {
    char buf[128];

    if (status == NF) {
        sprintf(buf, "HTTP/1.0 404 Not Found\r\n");
    } else if (status == OK) {
        sprintf(buf, "HTTP/1.0 200 OK\r\n");
    } else {
        sprintf(buf, "HTTP/1.0 500 Internal Servere Error\r\n");
    }

    sprintf(buf, "%sContent-Length: %lu\r\n\r\n", buf, content_length);

    size_t buf_len = strlen(buf);

    if (rio_writen(connfd, buf, buf_len) < buf_len) {
        fprintf(stderr, "error while sending response\n");
        return;
    }
    if (rio_writen(connfd, content, content_length) < content_length) {
        fprintf(stderr, "error while sending response\n");
    }
}

void send_file_response(int connfd, FILE *file) {
    fseek(file, 0L, SEEK_END);
    __off_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    if (file_size < 0) file_size = LONG_MAX;
    
    char header[64];
    char *buf = (char *)malloc(min(BUF_SIZE, file_size));
    if (buf == NULL) {
        perror("malloc");
    }
    sprintf(header, "HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", file_size);
    rio_writen(connfd, header, strlen(header));

    size_t readn;
    while ((readn = fread(buf, 1, BUF_SIZE, file)) > 0) {
        rio_writen(connfd, buf, readn);
    }

    free(buf);
}

int server(http_status_t *status) {
    int connfd = status->connfd;
    char *header = status->header;
    size_t readn = status->readn;

    int is_end = 0;

    while (1) {
        ssize_t ret = read(connfd, header + readn, 1);
        if (readn >= MAX_HEADER) {
            // entity too large
            free(header);
            free(status);
            send_response(connfd, ISE, content_500, sizeof(content_500));
            close(connfd);
            break;
        }
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // read end normally
                break;
            } else {
                perror("read");
                break;
            }
        } else if (ret == 0) {
            // EOF encountered
            break;
        } else {
            readn++;
            if ( readn >= 4
              && header[readn - 1] == '\n'
              && header[readn - 2] == '\r'
              && header[readn - 3] == '\n'
              && header[readn - 4] == '\r' ) {
                is_end = 1;
                break;
            }
        }
    }

    if (!is_end) {
        status->readn = readn;
        return 0;
    }

    request_t req_info;
    if (parse_request(header, &req_info) < 0) {
        send_response(connfd, ISE, content_500, strlen(content_500));
        return 1;
    }

    free(header);
    free(status);

    req_info.connfd = connfd;
    handle_request(&req_info);

    close(connfd);
    return 1;
}

void *thread(void *args) {
    struct epoll_event *events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAX_EVENTS);
    if (events == NULL) {
        perror("malloc");
    }
    struct epoll_event ev;
    int epollfd = ((struct thread_args*)args)->epollfd;
    int listenfd = ((struct thread_args*)args)->listenfd;

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);

    int nfds;
    while (1) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        if (nfds <= 0) {
            perror("epoll_wait");
            continue;
        }
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listenfd) {
                while (1) {
                    int connfd = accept(listenfd, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
                    if (connfd < 0) {
                        if (errno == EAGAIN | errno == EWOULDBLOCK) {
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    setnonblocking(connfd);

                    ev.events = EPOLLIN | EPOLLET;

                    http_status_t *status = (http_status_t *)malloc(sizeof(http_status_t));
                    char *header = (char *)malloc(MAX_HEADER);
                    if (header == NULL || status == NULL) {
                        perror("malloc");
                        exit(1);
                    }
                    status->header = header;
                    status->connfd = connfd;
                    status->readn = 0;
                    ev.data.ptr = status;

                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) < 0) {
                        perror("epoll_ctl");
                        continue;
                    }
                }
            } else {
                http_status_t *status = (http_status_t *)events[n].data.ptr;
                server(status);
            }
        }
    }

    free(events);
}

void setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
      
    fcntl(fd, F_SETFL, new_option);
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    int listenfd = open_listenfd(PORT);

    setnonblocking(listenfd);

    pthread_t threads[THREAD_NUM];
    
    int epollfd = epoll_create1(0);
    if (epollfd < 0) {
        fprintf(stderr, "error while creating epoll fd\n");
        exit(1);
    }

    struct thread_args targs;
    targs.epollfd = epollfd;
    targs.listenfd = listenfd;

    struct epoll_event epevent;
    epevent.events = EPOLLIN | EPOLLET;
    epevent.data.fd = listenfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &epevent) < 0) {
        fprintf(stderr, "error while adding listen fd to epoll inst\n");
        exit(1);
    }

    for (int i = 0; i < THREAD_NUM; ++i) {
        if (pthread_create(&threads[i], NULL, thread, &targs) < 0) {
            fprintf(stderr, "error while creating %d thread\n", i);
            exit(1);
        }
    }

    thread(&targs);

    close(epollfd);
    close(listenfd);
}