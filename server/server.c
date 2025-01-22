#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "server.h"

//서버 시작 함수
void start_server();
//전역 변수 정의
client_t *clients[MAX_CLIENTS] = {0};
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

core_number = 1;

int main() {
	start_server();
	return 0;
}

void start_server(){
	int server_socket, new_socket;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len = sizeof(client_addr);

	// 1. 서버 소켓 생성
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket < 0){
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	// 2. 서버 주소 설정
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY; //모든 네트워크 인터페이스에서 연결 허용

	// 3. 소켓 바인딩
	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Bind failed");
		close(server_socket);
		exit(EXIT_FAILURE);
	}


	// 클라이언트 연결 대기
	if(listen(server_socket, 10) < 0) { // 최대 10개 대기열
		perror("Listen failed");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	printf("Server is running on port %d\n", PORT);

	//뮤텍스로 코어 경쟁상태 방지
	pthread_mutex_t core_mutex = PTHREAD_MUTEX_INITIALIZER;
	
	pthread_mutex_lock(&core_mutex);
	if (core_number < 4) {
	    core_number++;
	} else {
	    core_number = 1;
	}
	pthread_mutex_unlock(&core_mutex);
	
	while(1) {
		new_socket = accept(server_socket, (struct sockaddr* )&client_addr, &client_len);
		if(new_socket < 0) {
			perror("Accept failed");
			continue; //다음 요청 처리
		}

		// 새로운 클라이언트 처리 : 메모리 할당, 스레드 생성
		client_t *new_client = (client_t*)malloc(sizeof(client_t));
		new_client->socket = new_socket;
		new_client->address = client_addr;

		pthread_t tid;			//스레드 ID 
		thread_args_t *args = (thread_args_t *)malloc(sizeof(thread_args_t));
		args->client = new_client;
		args->core_number = core_number; // 동적으로 코어 번호 전달
		
		pthread_create(&tid, NULL, handle_client, (void *)args);// 클라이언트 요청 처리 스레드 생성 (앞으로 여기서 계속 처리한다.
	
		// 스레드가 종료되면 리소스 자동으로 해제.
		pthread_detach(tid);
	}
}
