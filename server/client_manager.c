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
    thread_args_t *args = (thread_args_t *)arg;
    client_t *client = args->client;
    int core_number = args->core_number;
    free(args);

    printf("New client connected: %d\n", client->socket);

    // TCP_NODELAY 설정
    int flag = 1;
    setsockopt(client->socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    // CPU affinity 설정
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_number, &cpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("sched_setaffinity failed");
    }

    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes_received = recv(client->socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            printf("Client disconnected: %d\n", client->socket);
            close(client->socket);
            free(client);
            break;
        }

        buffer[bytes_received] = '\0';
        if (strncmp(buffer, "UPLOAD:", 7) == 0) {
            char *filename = buffer + 7;
            handle_file_upload(client->socket, filename);
        } else if (strncmp(buffer, "DOWNLOAD:", 9) == 0) {
            char *filename = buffer + 9;
            handle_file_download(client->socket, filename);
        } else {
            broadcast_message(buffer, client->socket);
        }
    }

    return NULL;
}

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

    send(client_socket, &file_stat.st_size, sizeof(file_stat.st_size), 0);
    sendfile(client_socket, file_fd, NULL, file_stat.st_size);

    close(file_fd);
    printf("File '%s' downloaded successfully\n", filename);
}
