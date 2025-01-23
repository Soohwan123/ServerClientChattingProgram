//#include <pthread.h>  <- Change it to epoll arcitecture
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "server.h"
#include <fcntl.h>
//서버 시작 함수
void start_server();
// 비차단 소켓 설정 함수 (비동기 epoll)
void set_nonblocking(int fd);

//전역 변수 정의
client_t *clients[MAX_CLIENTS] = {0};
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;



int main() {
	start_server();
	return 0;
}
// 비차단 소켓 설정 함수 (비동기 epoll)
void set_nonblocking(int fd) {
	//소켓 현재 플래그
	int flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1) {
		perror("fcntl F_GETFL failed");
		exit(EXIT_SUCCESS);
	}

	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 1) {// 소켓을 비차단모드로설정
		perror("fcntl F_GETFL failed");
		exit(EXIT_FAILURE);
	}
}


void start_server(){
	int server_socket, new_socket, epoll_fd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len = sizeof(client_addr);

	// 1. 서버 소켓 생성
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket < 0){
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}
	
	// 서버 소켓 비차단 설정
	set_nonblocking(server_socket);


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

	//뮤텍스로 코어 경쟁상태 방지 <- epoll 아키텍쳐로 교체
	//pthread_mutex_t core_mutex = PTHREAD_MUTEX_INITIALIZER;
	
	//pthread_mutex_lock(&core_mutex);
	//if (core_number < 4) {
	//    core_number++;
	//} else {
	//    core_number = 1;
	//}
	//pthread_mutex_unlock(&core_mutex);
	


	//epoll 인스턴스 생성
	epoll_fd = epoll_create1(0);
	if(epoll_fd == -1) {
		perror("epoll_create1 failed");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	//서버 소켓을 epoll 에 등록
	struct epoll_event event, events[MAX_EVENTS];
	event.events = EPOLLIN; // 읽기 이벤트 감지
	event.data.fd = server_socket; 	//서버 소켓 등록
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1){
		perror("epoll_ctl failed");
		close(server_socket);
		close(epoll_fd);
		exit(EXIT_FAILURE);
	}


	//epoll events
	while(1) {
		int n_ready = epoll_wait(epoll_fd, events, MAX_CLIENTS, -1);
		if(n_ready == -1) {
			perror("epoll_wait failed");
			break;
		}

		for(int i = 0; i < n_ready; i++) {
			if(events[i].data.fd == server_socket){
				// 새 클라이언트 연결 처리
				new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
				if(new_socket < 0){
					perror("Accept failed");
					continue;
				}

				printf("New client connected: %d\n", new_socket);

				// 클라이언트 소켓 비차단 설정
				set_nonblocking(new_socket);


				event.events = EPOLLIN | EPOLLET; // Edge-Triggered
				event.data.fd = new_socket;
				if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) {
					perror("epoll_ctl add client failed");
					close(new_socket);
				}
			} else {
				// 클라이언트 데이터 처리
				char buffer[BUFFER_SIZE];
				int client_fd = events[i].data.fd;// 이벤트 발생한 클라이언트 소켓

				int bytes_read = read(client_fd, buffer, sizeof(buffer));

				if(bytes_read <= 0) {
					//클라 종료
					printf("Client disconnected : %d\n", client_fd);
					close(client_fd);
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL); //epoll 에서 제거
				} else {
					buffer[bytes_read] = '\0';
					printf("Client %d sent : %s\n", client_fd, buffer);

					//다른 클라이언트들에게 메시지 브로드 캐스트
					broadcast_message(buffer, client_fd);
				}
			}
		}
	}

	close(server_socket);
	close(epoll_fd);
}

//멀티스레딩 아키텍쳐 -> 구버전
	
//		new_socket = accept(server_socket, (struct sockaddr* )&client_addr, &client_len);
//		if(new_socket < 0) {
//			perror("accept failed");
//			continue; //다음 요청 처리
//		}
//
//		// 새로운 클라이언트 처리 : 메모리 할당, 스레드 생성
//		client_t *new_client = (client_t*)malloc(sizeof(client_t));
//		new_client->socket = new_socket;
//		new_client->address = client_addr;
//
//		pthread_t tid;			//스레드 id 
//		thread_args_t *args = (thread_args_t *)malloc(sizeof(thread_args_t));
//		args->client = new_client;
//		args->core_number = core_number; // 동적으로 코어 번호 전달
//		
//		pthread_create(&tid, null, handle_client, (void *)args);// 클라이언트 요청 처리 스레드 생성 (앞으로 여기서 계속 처리한다.
//	
//		// 스레드가 종료되면 리소스 자동으로 해제.
//		pthread_detach(tid);
//	}
//v}
