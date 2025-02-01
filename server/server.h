#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netinet/in.h> //sockaddr_in
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>         // CPU affinity 설정
#include <netinet/tcp.h>   // TCP_NODELAY
#include <sys/stat.h>      // 파일 상태
#include <sys/sendfile.h>  // sendfile 함수
#include <openssl/sha.h> // WebSocket 핸드셰이크용
#include <stdint.h>
#include <sys/socket.h> //sendmmsg() recvmmsg() 로 변경
#include <liburing.h> // io_uring 사용



#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_BUFFER_SIZE 4096
#define MAX_CLIENTS 500
#define MAX_EVENTS 10000

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 500
#define MAX_EVENTS 10000
#define THREAD_POOL_SIZE 4 // Worker 스레드 개수
#define TASK_QUEUE_SIZE 100  // 작업 큐 크기

#define MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// 클라이언트 정보 구조체
typedef struct {
    int socket;
    struct sockaddr_in address; // 클라이언트 주소 정보
    int index;                  // 클라이언트 배열 내 인덱스
    int is_websocket;
} client_t;

// 스레드 풀 관련
typedef struct {
    int client_fd;
    char buffer[BUFFER_SIZE];
    int bytes_read;
} task_t;

extern client_t *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;


void start_server();
void set_nonblocking(int fd);
void add_client(int fd, struct sockaddr_in client_addr);
void remove_client(int epoll_fd, int fd);
void broadcast_message(int epoll_fd, char *message, int sender_socket);
void *worker_thread(void *arg);
void enqueue_task(int client_fd, const char *buffer, int bytes_read);
task_t dequeue_task();
client_t *find_client_by_fd(int fd);

#endif
