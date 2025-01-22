#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "server.h"


// 클라이언트들에게 메시지를 브로드캐스트
void broadcast_message(char *message, int sender_socket) {
	pthread_mutex_lock(&clients_mutex); // 잠금
	
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i] && clients[i]->socket != sender_socket){
			send(clients[i]->socket, message, strlen(message), 0);
		}
	}

	pthread_mutex_unlock(&clients_mutex); //잠금 해체
}
