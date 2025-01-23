#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h> //sockaddr_in
#include <pthread.h>
#include <sched.h>

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

void start_server();
void *handle_client(void *arg);
void broadcast_message(char *message, int sender_socket);

#endif
