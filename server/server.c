#include "server.h"
#include "websocket.h" // WebSocket 관련 함수 포함
#include <errno.h>
#include <asm-generic/socket.h>

// 함수 선언
void start_server();          // 서버 시작 함수
void set_nonblocking(int fd); // 비차단 소켓 설정 함수
void *worker_thread(void *arg);
void enqueue_task(int client_fd, const char *buffer, int bytes_read);
task_t dequeue_task();

// 전역 변수
client_t *clients[MAX_CLIENTS] = {0};                      // 클라이언트 배열 초기화
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // 클라이언트 배열 보호 뮤텍스

pthread_t workers[THREAD_POOL_SIZE]; // Worker 스레드 배열
task_t task_queue[TASK_QUEUE_SIZE];  // 작업 큐
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER; // 작업 큐 보호 뮤텍스
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;    // 작업 큐 조건 변수
int epoll_fd;                                           // epoll 파일 디스크립터
int task_count = 0;
int rear = 0;
int front = 0;

int main()
{
    start_server(); // 서버 실행
    return 0;
}

// 비차단 소켓 설정 함수
void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0); // 소켓의 현재 플래그 가져오기
    if (flags == -1)
    {
        perror("fcntl F_GETFL failed"); // 에러 메시지 출력
        exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {                                   // 소켓을 비차단 모드로 설정
        perror("fcntl F_SETFL failed"); // 에러 메시지 출력
        exit(EXIT_FAILURE);
    }
}

// 서버 시작 함수
void start_server()
{
    int server_socket, new_socket;   // 서버 소켓, 새 클라이언트 소켓, epoll 파일 디스크립터
    struct sockaddr_in server_addr, client_addr; // 서버 및 클라이언트 주소 구조체
    socklen_t client_len = sizeof(client_addr);

    // 1. 서버 소켓 생성
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 포트 재사용 옵션 추가
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 2. 서버 소켓 비차단 설정
    set_nonblocking(server_socket);

    // 3. 서버 주소 설정
    server_addr.sin_family = AF_INET;         // IPv4 사용
    server_addr.sin_port = htons(PORT);       // 포트 번호 설정
    server_addr.sin_addr.s_addr = INADDR_ANY; // 모든 네트워크 인터페이스에서 연결 허용

    // 4. 소켓 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 5. 클라이언트 연결 대기
    if (listen(server_socket, 10) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d\n", PORT);

    // 6. epoll 인스턴스 생성
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1 failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 7. 서버 소켓을 epoll에 등록
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        perror("epoll_ctl failed");
        close(server_socket);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    perror("Server started");
    printf("Server socket : %d\n", server_socket);

    // Worker 스레드 생성
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    // 8. epoll 이벤트 루프
    while (1)
    {
        int n_ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n_ready == -1)
        {
            perror("epoll_wait failed"); // epoll 대기 중 오류 발생 시 로그 출력
            break;                       // 루프 종료
        }

        for (int i = 0; i < n_ready; i++)
        {
            printf("Epoll event for fd: %d\n", events[i].data.fd);

            // 9. 서버 소켓 이벤트 처리
            if (events[i].data.fd == server_socket)
            {
                new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
                if (new_socket < 0)
                {
                    perror("Accept failed"); // 클라이언트 연결 실패 시 로그 출력
                    continue;                // 다음 이벤트 처리로 넘어감
                }
                printf("New client connected: %d\n", new_socket);
                set_nonblocking(new_socket);         // 새 클라이언트 소켓을 비차단 모드로 설정
                add_client(new_socket, client_addr); // 클라이언트 목록에 추가

                int flag = 1;
                setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)); // Nagle 알고리즘 비활성화

                event.events = EPOLLIN; // 입력 이벤트 등록
                event.data.fd = new_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1)
                {
                    perror("epoll_ctl add client failed"); // epoll 제어 실패 시 로그 출력
                    close(new_socket);                     // 소켓 닫기
                }
            }
            // 클라이언트 데이터 처리
            else {
                int client_fd = events[i].data.fd;

                client_t *client = find_client_by_fd(client_fd);
                if (!client) {
                    printf("Error: Client fd %d not found in clients array (likely removed)\n", client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    continue;
                }

                char buffer[BUFFER_SIZE];
                int bytes_read = read(client_fd, buffer, sizeof(buffer));
                printf("Received message from client %d: %s\n", client_fd, buffer);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    if (strncmp(buffer, "GET / HTTP/1.1", 14) == 0) {
                        // WebSocket 핸드셰이크 처리
                        const char *key = extract_websocket_key(buffer);

                        if (key) {
                            websocket_handshake(client_fd, key);
                            client->is_websocket = 1;
                        }
                    } else {
                        enqueue_task(client_fd, buffer, bytes_read); // 메시지 큐에 저장
                    }
                }else if (bytes_read == 0) {
                    printf("Client %d disconnected\n", client_fd);
                    remove_client(epoll_fd, client_fd); // 클라이언트 연결 끊음
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("Read error");
                    remove_client(epoll_fd, client_fd); // 오류 발생 시 클라이언트 연결 끊음
                }

            }
        }
    }
    close(server_socket);
    close(epoll_fd);
}

void *worker_thread(void *arg) {
    while (1) {
        task_t task = dequeue_task();
        if (task.client_fd == -1) continue;

        pthread_mutex_lock(&clients_mutex);
        client_t *client = find_client_by_fd(task.client_fd);
        if (!client) {
            printf("Error: Client fd %d not in clients array (likely removed)\n", task.client_fd);
            pthread_mutex_unlock(&clients_mutex);
            continue; // 제거된 클라이언트이면 스킵
        }
        pthread_mutex_unlock(&clients_mutex);

        char *buffer = task.buffer;
        int bytes_read = task.bytes_read;

        if (client->is_websocket) {
            size_t processed = 0;

            while (processed < bytes_read) {
                char decoded_message[BUFFER_SIZE];
                size_t decoded_length = decode_websocket_frame(buffer + processed, bytes_read - processed, decoded_message, sizeof(decoded_message));

                if (decoded_length > 0) {
                    websocket_broadcast(decoded_message, task.client_fd);
                    processed += decoded_length;  // 다음 프레임으로 이동
                } else {
                    // 불완전한 프레임이면 대기 (추가 데이터 수신 필요)
                    break;
                }
            }
        }else { // wrk 테스트용
            if (strncmp(buffer, "GET / HTTP/1.1", 14) == 0) {
                const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nConnection Clear!";
                send(task.client_fd, response, strlen(response), 0);
            }
        }
    }
    return NULL;
}

void enqueue_task(int client_fd, const char *buffer, int bytes_read) {
    pthread_mutex_lock(&queue_mutex);
    if (task_count < TASK_QUEUE_SIZE) {
        memset(&task_queue[rear], 0, sizeof(task_t));

        task_queue[rear].client_fd = client_fd;
        memcpy(task_queue[rear].buffer, buffer, bytes_read);
        task_queue[rear].buffer[bytes_read] = '\0';
        task_queue[rear].bytes_read = bytes_read;

        rear = (rear + 1) % TASK_QUEUE_SIZE;    //Circular queue
        task_count++;
        pthread_cond_signal(&queue_cond);
    }
    pthread_mutex_unlock(&queue_mutex);
}

task_t dequeue_task()
{
    pthread_mutex_lock(&queue_mutex);
    while (task_count == 0)
    {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    task_t task = task_queue[front];
    front = (front + 1) % TASK_QUEUE_SIZE;

    task_count--;
    pthread_mutex_unlock(&queue_mutex);
    return task;
}

// 멀티스레딩 아키텍쳐 -> 구버전

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
// v}
