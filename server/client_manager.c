#include <sched.h>         // CPU affinity 설정
#include <unistd.h>        // POSIX API
#include <netinet/in.h>    // sockaddr_in
#include <netinet/tcp.h>   // TCP_NODELAY
#include <stdio.h>         // 입출력
#include <stdlib.h>        // 동적 메모리 할당
#include <string.h>        // 문자열 처리
#include <sys/stat.h>      // 파일 상태
#include <sys/sendfile.h>  // sendfile 함수
#include <fcntl.h>         // 파일 제어
#include "server.h"        // 서버 정의

void handle_file_upload(int client_socket, const char *filename);
void handle_file_download(int client_socket, const char *filename);

// 클라이언트 요청을 처리하는 스레드 함수
void *handle_client(void *arg) {
    	thread_args_t *args = (thread_args_t *)arg; // 전달된 인자를 구조체로 캐스팅
    	client_t *client = args->client;           // 클라이언트 정보 추출
    	int core_number = args->core_number;       // 코어 번호 추출
	char buffer[BUFFER_SIZE];
	free(args); // 동적으로 할당된 구조체 메모리 해제

	printf("New client connected : %d\n", client->socket);


	// Low Latency 최적화 (TCP_NODELAY) 설정
	int flag = 1;
	setsockopt(client->socket,IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)); 

	// Low Latency 최적화 CPU 고정 ( 버전 오류로 주석처리 )
	// cpu_set_t cpuset;
	// CPU_ZERO(&cpuset);
	// CPU_SET(core_number, &cpuset); // 동적으로 지정된 코어 번호
	// pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);



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

		//클라이언트 요청 확인, 처리하는
		if(strncmp(buffer, "UPLOAD:", 7) == 0) {
			char *filename = buffer + 7;
			handle_file_upload(client->socket, filename);
		} else if(strncmp(buffer, "DOWNLOAD:" , 9) == 0){
			char *filename = buffer + 9;
			handle_file_download(client->socket, filename);
		} else { //일반메시지
		// 메시지를 다른 클라이언트들에게 브로드캐스트
			broadcast_message(buffer, client->socket);
		}
	}

	return NULL;
}



// 클라이언트 업로드 파일 처리
void handle_file_upload(int client_socket, const char *filename) {
	char filepath[256];
	snprintf(filepath,sizeof(filepath), "uploads/%s", filename); // 업로드 디렉토리에 파일저장 경로 설정


	FILE *file = fopen(filepath, "wb");
	if(!file) {
		perror("File open failed");
		return;
	}

	char buffer[FILE_BUFFER_SIZE];
	int bytes_received;
	while((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
		fwrite(buffer, 1, bytes_received, file);

	}

	fclose(file);
	printf("File '%s' uploaded successfully\n", filename);
}


// 클라이언트 다운로드 파일 처리

void handle_file_download(int client_socket, const char *filename) {
	char filepath[256];
	snprintf(filepath,sizeof(filepath), "uploads/%s", filename); // 업로드 디렉토리에 파일저장 경로 설정


	int file_fd = open(filepath, O_RDONLY); 
	if(!file_fd < 0) {
		perror("File open failed");
		return;
	}

	struct stat file_stat;
	fstat(file_fd, &file_stat); //파일 크기 확인

	// 파일 크기를 클라이언트에게 전송
	send(client_socket, &file_stat.st_size, sizeof(file_stat.st_size), 0);

	//파일 내용 전송
	sendfile(client_socket, file_fd, NULL, file_stat.st_size);

	close(file_fd);
	printf("File '%s' downloaded successfully\n", filename);
}
