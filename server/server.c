#include "server.h"
#include "utils.c" // 유틸리티 함수 포함
#include <errno.h>

// 함수 선언
void start_server();                                  // 서버 시작 함수
void set_nonblocking(int fd);                        // 비차단 소켓 설정 함수

// 전역 변수
client_t *clients[MAX_CLIENTS] = {0};                // 클라이언트 배열 초기화
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // 클라이언트 배열 보호 뮤텍스

int main() {
    start_server(); // 서버 실행
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
    server_addr.sin_port = htons(PORT);             // 포트 번호 설정
    server_addr.sin_addr.s_addr = INADDR_ANY;       // 모든 네트워크 인터페이스에서 연결 허용

    // 4. 소켓 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 5. 클라이언트 연결 대기
    if (listen(server_socket, 10) < 0) {            
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d\n", PORT);

    // 6. epoll 인스턴스 생성
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 7. 서버 소켓을 epoll에 등록
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;                         
    event.data.fd = server_socket;                  
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
        perror("epoll_ctl failed");
        close(server_socket);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    perror("Server started");
    printf("Server socket : %d\n", server_socket);

    // 8. epoll 이벤트 루프
    while (1) {
        int n_ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); 
        if (n_ready == -1) {
            perror("epoll_wait failed"); // epoll 대기 중 오류 발생 시 로그 출력
            break;  // 루프 종료                         
        }

        for (int i = 0; i < n_ready; i++) {
            printf("Epoll event for fd: %d\n", events[i].data.fd);

            // 9. 서버 소켓 이벤트 처리
            if (events[i].data.fd == server_socket) {
                new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
                if (new_socket < 0) {
                    perror("Accept failed"); // 클라이언트 연결 실패 시 로그 출력
                    continue;  // 다음 이벤트 처리로 넘어감
                }
                printf("New client connected: %d\n", new_socket);
                set_nonblocking(new_socket); // 새 클라이언트 소켓을 비차단 모드로 설정
                add_client(new_socket, client_addr); // 클라이언트 목록에 추가

                int flag = 1;
                setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)); // Nagle 알고리즘 비활성화

                event.events = EPOLLIN; // 입력 이벤트 등록
                event.data.fd = new_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) {
                    perror("epoll_ctl add client failed"); // epoll 제어 실패 시 로그 출력
                    close(new_socket); // 소켓 닫기
                }
            } 
            // 클라이언트 데이터 처리
            else {
                char buffer[BUFFER_SIZE];
                int client_fd = events[i].data.fd;
                client_t *client = find_client_by_fd(client_fd);
                if (!client) {
                    printf("Error: Client fd %d not in clients array\n", client_fd);
                    remove_client(client_fd); // 클라이언트 목록에서 제거
                    continue; // 다음 이벤트 처리로 넘어감
                }

                int bytes_read;
                while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
                    buffer[bytes_read] = '\0'; // 문자열 종료 처리
		    printf("Raw message recieved (length: %d) : %s\n", bytes_read, buffer);// 디버깅 출력
                    // WebSocket 핸드셰이크 처리
                    if (strstr(buffer, "Upgrade: websocket")) {
                        char *websocket_key = extract_websocket_key(buffer);
                        if (websocket_key) {
                            websocket_handshake(client_fd, websocket_key); // WebSocket 핸드셰이크 처리
                        } else {
                            printf("WebSocket key not found in request.\n");
                        }
                        continue;
                    }

                    // WebSocket 메시지 처리
                    if (buffer[0] == 0x81) { // WebSocket 메시지인지 확인
                        char decoded_message[BUFFER_SIZE];
                        int decoded_length = decode_websocket_frame(buffer, bytes_read, decoded_message, sizeof(decoded_message));
                        if (decoded_length > 0) {
                            decoded_message[decoded_length] = '\0';
                            printf("WebSocket message received: %s\n", decoded_message);

                            // WebSocket 브로드캐스트
                            websocket_broadcast(decoded_message, client_fd);
                        } else {
                            printf("Failed to decode WebSocket message from client %d.\n", client_fd);
                        }
                    }

                    // 일반 HTTP 요청 처리
                    else if (strncmp(buffer, "GET / HTTP/1.1", 14) == 0) {
                        const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nConnection Clear!";
                        if (send(client_fd, response, strlen(response), 0) == -1) {
                            perror("Failed to send HTTP response");
                        }
                    }

                    // 버퍼 초기화
                    memset(buffer, 0, BUFFER_SIZE);
                }

                if (bytes_read == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        // EAGAIN이 아니면 클라이언트 연결 종료 처리
                        perror("Read error");
                        printf("Client disconnected: %d\n", client_fd);
                        remove_client(client_fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    } else {
                        // EAGAIN: 데이터가 아직 준비되지 않음
                        printf("EAGAIN received for client %d\n", client_fd);
                    }
                } else if (bytes_read == 0) {
                    // 클라이언트가 연결을 종료함
                    printf("Client disconnected: %d\n", client_fd);
                    remove_client(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
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
