#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "server.h"



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

// 메시지 브로드캐스트
void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex); // 뮤텍스 잠금

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket != sender_socket) {
            if (send(clients[i]->socket, message, strlen(message), 0) < 0) {
                perror("Send failed");
                remove_client(clients[i]->socket);  // **수정**: 직접 메모리 해제 대신 remove_client 호출
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex); // 뮤텍스 잠금 해제
}

// 클라이언트 추가 함수
void add_client(int fd, struct sockaddr_in client_addr) {
    if (find_client_by_fd(fd)) {                    // **수정**: 중복 확인 추가
        printf("Warning: Client %d already exists in clients array\n", fd);
        return;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = malloc(sizeof(client_t));
            clients[i]->socket = fd;
            clients[i]->address = client_addr;
            clients[i]->index = i;                  // 배열 인덱스 저장
            printf("Client %d added to clients array at index %d\n", fd, i);
            return;
        }
    }
    printf("Error: No available slot for client %d\n", fd);
}

// 클라이언트 제거 함수
void remove_client(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == fd) {
            close(clients[i]->socket);              // 소켓 닫기
            free(clients[i]);                       // 메모리 해제
            clients[i] = NULL;                      // 슬롯 초기화
            printf("Client fd %d removed from clients array\n", fd);
            return;
        }
    }
}

// 클라이언트 찾기 함수
client_t* find_client_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == fd) {
            return clients[i];
        }
    }
    return NULL;
}
