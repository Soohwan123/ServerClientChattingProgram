#include <netinet/in.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h> //파일 크기확인
#include <sys/sendfile.h> //sendfile 함수
#include "server.h"
#include <sched.h>


void handle_file_upload(int client_socket, const char *filename);
void handle_file_download(int client_socket, const char *filename);

// 클라이언트 요청을 처리하는 스레드 함수
void *handle_client(void *arg, int core_number) {
	char buffer[BUFFER_SIZE];
	client_t *client = (client_t*) arg; //인자로 받은 클라이언트 정보
	
	printf("New client connected : %d\n", client->socket);
	

	// Low Latency 최적화 (TCP_NODELAY) 설정
	int flag = 1;
	setsockopt(client->socket,IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)); 

	// Low Latency 최적화 CPU 고정
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_number, &cpuset); // 2번코어에 스레드 고정 (캐시사용 극대화, 컨텍스 스위칭 감소
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);



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
