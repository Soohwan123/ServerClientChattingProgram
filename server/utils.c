#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "server.h"


// 메시지 브로드캐스트
void broadcast_message(int epoll_fd, char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex); // 뮤텍스 잠금

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket != sender_socket) {
            if (send(clients[i]->socket, message, strlen(message), 0) < 0) {
                perror("Send failed");
                remove_client(epoll_fd, clients[i]->socket);  // **수정**: 직접 메모리 해제 대신 remove_client 호출
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex); // 뮤텍스 잠금 해제
}

// 클라이언트 추가 함수
void add_client(int fd, struct sockaddr_in client_addr) {
    pthread_mutex_lock(&clients_mutex);
    if (find_client_by_fd(fd)) {
        printf("Warning: Client %d already exists in clients array\n", fd);
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = malloc(sizeof(client_t));
            clients[i]->socket = fd;
            clients[i]->address = client_addr;
            clients[i]->index = i;
	    clients[i]->is_websocket = 0;
            printf("Client %d added to clients array at index %d\n", fd, i);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    printf("Error: No available slot for client %d\n", fd);
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int epoll_fd, int fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == fd) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL); // **epoll에서 제거**
            close(clients[i]->socket);
            free(clients[i]);
            clients[i] = NULL;
            printf("Client fd %d removed from clients array\n", fd);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
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