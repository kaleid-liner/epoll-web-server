# Epoll Web Server

A simple yet high performance web server written with epoll and pure c.

## Compile

Type in folder [/files](./files)

```bash
make
```

## Run

```
./server
```

## Code Design

### Concurrency

Concurrency of this web server was implemented by using thread pool (Posix thread) and epoll.

#### Steps

1. The main thread finish initializing work (e.g., configing network, listening on socket).

2. The main thread creates an epoll instance and adds the listen file descriptor to the interest list of the epoll instance (**Edge Triggered**).

3. The main thread creates `THREAD_NUM` threads (`epollfd` and `listenfd` will be passed as arguments). Then both the main thread and the child threads will serve as workers.

4. All the workers call `epoll_wait` on the epoll instance. 

5. When any event is caught, the worker thread that gets it will do:

   1. Check if it's `listenfd`. If It is `listenfd`, then call `accept` on it and add `connfd` (return value of `accept`) to the interest list.

   2. Else it is a `connfd`. `read` on it. If the header transport is done (a blank new line detected), then respond to it. Else store the status at `event.data.ptr`.

      The status is defined as:

      ```c
      typedef struct HttpStatus {
          int connfd;     // connection file descriptor
          char *header;   // http header read, malloced with `MAX_HEADER` size
          size_t readn;   // number of bytes read
          FILE *file;     // file to send
          size_t left;    // number of bytes left to send
          req_status_t req_status; 
      } http_status_t;
      ```

      While `req_status_t` is defined as:

      ```c
      typedef enum REQUEST_STATUS {
          Reading,
          Writing,
          Ended
      } req_status_t;
      ```

      Next time when `epoll_wait` get events on this fd, the server will continue on the request.

   3. After complete reading, connfd will enter status `Writing`. If `sendfile` cause `EAGAIN`, and `left > 0` , it means that writing end is temporily unavailable. I have to save the status, `EPOLL_CTL_MOD` to change its trigger events to `EPOLLOUT | EPOLLET`. And continue the writing next time. 

### Notice

- While calling `read` or `sendfile` on a nonblocking descriptor, only when you get a `-1` return value and `errno == EAGAIN || errno == EWOULDBLOCK` (no data left) or a `0` return value (EOF detected), it means that the reading is done.

  You may get a `0` return value when:

  - Client close the connection. At this case the client will send a EOF implicitly.
  - Client shutdown the writing end of the connection.
  - Client explicitly send a EOF to signal the server it has finished writing.

- You can only use Edge Triggered mode in multi-thread environments (**when you share an epoll instance among threads**) to prevent spurious wake-ups.

  *Why I share an epoll instance among threads:* Because this is more performant. And `epoll_wait` is generally thread-safe. ET also overperfoms LT.

- `epoll_data_t.data` is a union. Its `ptr` field is designed to store session state. You should malloc status when needed and assign it to the `ptr`, and free it after you have done responding to it.

- Notice that writing to connfd (by calling `sendfile`) is also non-blocking.

- **USE EPOLLONESHOT** in multithreaded environment.

## Features

- The server operates smoothly when network is bad. I simulate this situation by `write`ing half of the content, sleeping for 10 seconds, and `write`ing the remaining half.
- The performance, concurrency and availability seems well on my dual core virtual machine.
- Basic access control by comparing the absolute path of the request file and workspace of the server.

## Load Test

Test on my dual core ubuntu virtual machine: 

![server_load_test](assets/server_load_test.png)