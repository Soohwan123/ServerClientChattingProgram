#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "server.h"


// 클라이언트 요청을 처리하는 스레드 함수
void *handle_client(void *arg) {
	char buffer[BUFFER_SIZE];
	client_t *client = (client_t*) arg; //인자로 받은 클라이언트 정보
	
	printf("New client connected : %d\n", client->socket);

	while(1) {
		// 데이터 수신
		int bytes_received = recv(client->socket, buffer, sizeof(buffer), 0);
		// 수신 실패 또는 클라이언트종료
		if(bytes_received <= 0){
			printf("Client disconnected : %d\n", client->socket);
			close(client->socket);
			free(client);
			break;
		}

		buffer[bytes_received] = '\0'; //문자열처리
		printf("Client %d: %s\n", client->socket, buffer);

		// 메시지를 다른 클라이언트들에게 브로드캐스트
		broadcast_message(buffer, client->socket);
	}

	return NULL;
}




