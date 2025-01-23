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

#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_BUFFER_SIZE 4096
#define MAX_CLIENTS 500
#define MAX_EVENTS 10000

// 클라이언트 정보 구조체
typedef struct {
	int socket;
	struct sockaddr_in address;   //클라이언트 주소정보
	char username[32];
	int index;                // 배열 내 인덱스 (추적용)
}client_t;

typedef struct {
    client_t *client;     // 클라이언트 정보
    int core_number;      // 할당된 코어 번호
} thread_args_t;


extern client_t *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;
extern int core_number;

void handle_file_upload(int client_socket, const char *filename); // 파일 업로드 처리
void handle_file_download(int client_socket, const char *filename); // 파일 다운로드 처리
void broadcast_message(char *message, int sender_socket);          // 메시지 브로드캐스트
void add_client(int fd, struct sockaddr_in client_addr);           // 클라이언트 추가
void remove_client(int fd);                                         // 클라이언트 제거
client_t* find_client_by_fd(int fd);  				   // fd 로 클라이언트 찾기 
void start_server();
void *handle_client(void *arg);


#endif
