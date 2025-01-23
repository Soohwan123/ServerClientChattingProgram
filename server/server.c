//#include <pthread.h>  <- Change it to epoll architecture
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "server.h"
#include <fcntl.h>
#include <sched.h>         // CPU affinity 설정
#include <netinet/tcp.h>   // TCP_NODELAY
#include <sys/stat.h>      // 파일 상태
#include <sys/sendfile.h>  // sendfile 함수


void start_server();                                  // 서버 시작 함수
void set_nonblocking(int fd);                        // 비차단 소켓 설정 함수
void handle_file_upload(int client_socket, const char *filename); // 파일 업로드 처리
void handle_file_download(int client_socket, const char *filename); // 파일 다운로드 처리
void broadcast_message(char *message, int sender_socket);          // 메시지 브로드캐스트

client_t *clients[MAX_CLIENTS] = {0};                // 클라이언트 배열 초기화
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // 클라이언트 배열 보호 뮤텍스

int main() {
    start_server();                                  // 서버 실행
    return 0;
}

// 비차단 소켓 설정 함수
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);              // 소켓의 현재 플래그 가져오기
    if (flags == -1) {
        perror("fcntl F_GETFL failed");             // 에러 메시지 출력
        exit(EXIT_FAILURE);
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) { // 소켓을 비차단 모드로 설정
        perror("fcntl F_SETFL failed");             // 에러 메시지 출력
        exit(EXIT_FAILURE);
    }
}

// 파일 업로드 처리
void handle_file_upload(int client_socket, const char *filename) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);

    FILE *file = fopen(filepath, "wb");
    if (!file) {
        perror("File open failed");
        return;
    }

    char buffer[FILE_BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("File '%s' uploaded successfully\n", filename);
}

// 파일 다운로드 처리
void handle_file_download(int client_socket, const char *filename) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        perror("File open failed");
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);

    // 파일 크기를 클라이언트에게 전송
    send(client_socket, &file_stat.st_size, sizeof(file_stat.st_size), 0);

    // 파일 내용 전송
    sendfile(client_socket, file_fd, NULL, file_stat.st_size);

    close(file_fd);
    printf("File '%s' downloaded successfully\n", filename);
}

void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex); // 뮤텍스 잠금

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket != sender_socket) {
            if (send(clients[i]->socket, message, strlen(message), 0) < 0) {
                perror("Send failed");
                close(clients[i]->socket);
                free(clients[i]);       // 메모리 해제
                clients[i] = NULL;      // 슬롯 초기화
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex); // 뮤텍스 잠금 해제
}

client_t* find_client_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == fd) {
            return clients[i];
        }
    }
    return NULL;
}

// 서버 시작 함수
void start_server() {
    int server_socket, new_socket, epoll_fd;        // 서버 소켓, 새 클라이언트 소켓, epoll 파일 디스크립터
    struct sockaddr_in server_addr, client_addr;    // 서버 및 클라이언트 주소 구조체
    socklen_t client_len = sizeof(client_addr);     
	
    // 1. 서버 소켓 생성
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");           
        exit(EXIT_FAILURE);
    }

    // 2. 서버 소켓 비차단 설정
    set_nonblocking(server_socket);

    // 3. 서버 주소 설정
    server_addr.sin_family = AF_INET;               // IPv4 사용
    server_addr.sin_port = htons(PORT);             // 포트 번호 설정 (네트워크 바이트 순서로 변환)
    server_addr.sin_addr.s_addr = INADDR_ANY;       // 모든 네트워크 인터페이스에서 연결 허용

    // 4. 소켓 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");                      
        close(server_socket);                       
        exit(EXIT_FAILURE);
    }

    // 5. 클라이언트 연결 대기
    if (listen(server_socket, 10) < 0) {            // 최대 10개 연결 대기열
        perror("Listen failed");                    // 연결 대기 실패 시 에러 출력
        close(server_socket);                       // 소켓 닫기
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d\n", PORT);

    // 6. epoll 인스턴스 생성
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");             // epoll 인스턴스 생성 실패
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 7. 서버 소켓을 epoll에 등록
    struct epoll_event event, events[MAX_EVENTS];   // epoll 이벤트 구조체 배열
    event.events = EPOLLIN;                         // 읽기 이벤트 감지
    event.data.fd = server_socket;                  // 서버 소켓 등록
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
        perror("epoll_ctl failed");                 // epoll 등록 실패 시 에러 출력
        close(server_socket);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    // 8. epoll 이벤트 루프
    while (1) {
        int n_ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); // 이벤트 대기 -> 시작된 이벤트 갯수 return
        if (n_ready == -1) {
            perror("epoll_wait failed");            
            break;                                  
        }

        for (int i = 0; i < n_ready; i++) {
	    printf("Epoll event for fd: %d\n", events[i].data.fd);
		//서버 소켓이라면
            if (events[i].data.fd == server_socket) {
                // 새 클라이언트 연결 처리
                new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
                if (new_socket < 0) {
                    perror("Accept failed");        
                    continue;                       // 다음 이벤트로 넘어감
                }

                printf("New client connected: %d\n", new_socket);

		// 새 클라이언트 연결 처리
		for (int i = 0; i < MAX_CLIENTS; i++) {
		    if (clients[i] == NULL) {
		        clients[i] = malloc(sizeof(client_t));
		        clients[i]->socket = new_socket;
		        clients[i]->address = client_addr;
		        printf("Client %d added to clients array at index %d\n", new_socket, i);
		        break;
		    }
		}

                // 클라이언트 소켓 비차단 설정
                set_nonblocking(new_socket);

                // TCP_NODELAY 설정 (Low Latency)
                int flag = 1;
                setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

		// Low Latency 최적화 CPU 고정 ( 버전 오류로 주석처리 )
		// cpu_set_t cpuset;
		// CPU_ZERO(&cpuset);
		// CPU_SET(core_number, &cpuset); // 동적으로 지정된 코어 번호
		// pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

                // 새 클라이언트 소켓을 epoll에 등록
                event.events = EPOLLIN | EPOLLET;  // 읽기 및 Edge-Triggered 모드 설정
                event.data.fd = new_socket;       // 클라이언트 소켓 등록
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) {
                    perror("epoll_ctl add client failed"); // 등록 실패 시 에러 출력
                    close(new_socket);           
                }
            } else {
                // 클라이언트 데이터 처리
                char buffer[BUFFER_SIZE];
                int client_fd = events[i].data.fd; // 이벤트 발생한 클라이언트 소켓
                client_t *client = find_client_by_fd(client_fd);
		if (!client) {
                    printf("Error: Client fd %d not in clients array\n", client_fd);
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    continue;
                }
		    
		if (clients[client_fd] == NULL) {
			printf("Error: Client fd %d not in clients array\n", client_fd);
			close(client_fd);
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
			continue;
		}
                int bytes_read = read(client_fd, buffer, sizeof(buffer));

                if (bytes_read <= 0) {
                    // 클라이언트 종료
                    printf("Client disconnected: %d\n", client_fd);
		    // 클라이언트 종료 처리
		    for (int i = 0; i < MAX_CLIENTS; i++) {
			    if (clients[i] && clients[i]->socket == client_fd) {
			        free(clients[i]);        // 메모리 해제
			        clients[i] = NULL;       // 슬롯 초기화
			        break;
			    }
			}
                    close(client_fd);             // 소켓 닫기
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL); // epoll에서 제거
                } else {
                    buffer[bytes_read] = '\0';    // 문자열 종료
                    printf("Client %d sent: %s\n", client_fd, buffer);

                    // 클라이언트 요청 처리
                    if (strncmp(buffer, "UPLOAD:", 7) == 0) {
                        handle_file_upload(client_fd, buffer + 7); // 파일 업로드 처리
                    } else if (strncmp(buffer, "DOWNLOAD:", 9) == 0) {
                        handle_file_download(client_fd, buffer + 9); // 파일 다운로드 처리
                    } else {
                        broadcast_message(buffer, client_fd); // 메시지 브로드캐스트
                    }

		     memset(buffer, 0, BUFFER_SIZE); // 매 요청 후 버퍼 초기화
                }
            }
        }
    }

    close(server_socket);                          // 서버 소켓 닫기
    close(epoll_fd);                               // epoll 파일 디스크립터 닫기
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
