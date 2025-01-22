#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h> //sockaddr_in
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_BUFFER_SIZE 4096
#define MAX_CLIENTS 500

// 클라이언트 정보 구조체
typedef struct {
	int socket;
	struct sockaddr_in address;   //클라이언트 주소정보
	char username[32];
}client_t;


extern client_t *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

void start_server();
void *handle_client(void *arg);
void broadcast_message(char *message, int sender_socket);

#endif
