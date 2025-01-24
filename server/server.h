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

#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_BUFFER_SIZE 4096
#define MAX_CLIENTS 500
#define MAX_EVENTS 10000

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 500
#define MAX_EVENTS 10000
#define MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// 클라이언트 정보 구조체
typedef struct {
    int socket;
    struct sockaddr_in address; // 클라이언트 주소 정보
    int index;                  // 클라이언트 배열 내 인덱스
} client_t;


extern client_t *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;


void start_server();
void set_nonblocking(int fd);
void websocket_handshake(int client_fd, const char *sec_websocket_key);
size_t decode_websocket_frame(const char *frame, size_t length, char *decoded_message, size_t buffer_size);
size_t encode_websocket_frame(const char *message, char *frame, size_t buffer_size);
void websocket_broadcast(const char *message, int sender_socket);
void add_client(int fd, struct sockaddr_in client_addr);
void remove_client(int fd);
client_t *find_client_by_fd(int fd);
char *extract_websocket_key(const char *buffer);
void base64_encode(const unsigned char *input, size_t length, char *output, size_t output_size);
int websocket_decode_message(const char *input, char *output, size_t input_len);

#endif
