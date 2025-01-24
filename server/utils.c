#include <pthread.h>
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

void remove_client(int fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == fd) {
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


// WebSocket 핸드셰이크 응답 함수
void websocket_handshake(int client_fd, const char *sec_websocket_key) {
    char key_magic[256];
    snprintf(key_magic, sizeof(key_magic), "%s%s", sec_websocket_key, MAGIC_STRING);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)key_magic, strlen(key_magic), hash);

    char accept_key[256];
    base64_encode(hash, SHA_DIGEST_LENGTH, accept_key, sizeof(accept_key));

    char response[512];
    snprintf(response, sizeof(response),
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n",
             accept_key);

    send(client_fd, response, strlen(response), 0);
    printf("WebSocket handshake completed with client %d\n", client_fd);
}

size_t decode_websocket_frame(const char *frame, size_t length, char *decoded_message, size_t buffer_size) {
    if (length < 2) {
        printf("WebSocket frame too short\n");
        return 0; // 최소 길이 확인
    }

    size_t payload_length = frame[1] & 0x7F; // 첫 7비트에서 페이로드 길이 추출
    size_t offset = 2; // 기본 오프셋

    if (payload_length == 126) {
        if (length < 4) {
            printf("WebSocket frame too short for extended payload\n");
            return 0;
        }
        payload_length = (frame[2] << 8) | frame[3];
        offset += 2;
    } else if (payload_length == 127) {
        printf("WebSocket frame with 8-byte payload not supported\n");
        return 0;
    }

    if (length < offset + 4) {
        printf("WebSocket frame too short for masking key\n");
        return 0;
    }
    const uint8_t *masking_key = (uint8_t *)&frame[offset];
    offset += 4;

    if (length < offset + payload_length) {
        printf("WebSocket frame too short for payload\n");
        return 0;
    }
    const uint8_t *payload = (uint8_t *)&frame[offset];

    if (payload_length > buffer_size - 1) {
        printf("Payload length exceeds buffer size\n");
        return 0;
    }

    for (size_t i = 0; i < payload_length; i++) {
        decoded_message[i] = payload[i] ^ masking_key[i % 4];
    }
    decoded_message[payload_length] = '\0';

    printf("Decoded WebSocket message: %s\n", decoded_message);
    return payload_length;
}


// WebSocket 프레임 디코딩 함수
int websocket_decode_message(const char *input, char *output, size_t input_len) {
    if (input_len < 2) return -1; // 최소 길이 체크

    size_t payload_length = input[1] & 0x7F; // 첫 7비트는 payload 길이
    size_t offset = 2;

    // Extended payload length 처리
    if (payload_length == 126) {
        payload_length = (input[2] << 8) | input[3];
        offset += 2;
    } else if (payload_length == 127) {
        // 8바이트 길이 (잘 사용되지 않음)
        payload_length = 0; // 대규모 payload는 처리하지 않음
        return -1;
    }

    // Mask 확인
    uint8_t masking_key[4];
    memcpy(masking_key, input + offset, 4);
    offset += 4;

    // Payload 디코딩
    for (size_t i = 0; i < payload_length; i++) {
        output[i] = input[offset + i] ^ masking_key[i % 4];
    }
    output[payload_length] = '\0'; // 문자열 종료

    return payload_length; // 디코딩된 메시지 길이 반환
}

// WebSocket 메시지 인코딩 함수
size_t encode_websocket_frame(const char *message, char *frame, size_t buffer_size) {
    size_t message_length = strlen(message);
    if (message_length > buffer_size - 2) {
        printf("Message too large to encode\n");
        return 0;
    }

    // WebSocket 헤더 생성
    frame[0] = 0x81;  // FIN 플래그와 텍스트 프레임
    frame[1] = message_length;

    // 메시지 복사
    memcpy(&frame[2], message, message_length);
    return message_length + 2;
}

// WebSocket 브로드캐스트 함수
void websocket_broadcast(const char *message, int sender_socket) {
    printf("Entering websocket_broadcast: message='%s', sender_socket=%d\n", message, sender_socket);
    
    pthread_mutex_lock(&clients_mutex); // 뮤텍스 잠금
    printf("Mutex locked in websocket_broadcast\n");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket != sender_socket) {
            printf("Attempting to send to client at index %d, socket=%d\n", i, clients[i]->socket);
            
            char frame[BUFFER_SIZE];
            size_t frame_length = encode_websocket_frame(message, frame, sizeof(frame));
            if (frame_length > 0) {
                if (send(clients[i]->socket, frame, frame_length, 0) == -1) {
                    perror("WebSocket send failed");
                } else {
                    printf("Sent to client %d: %s\n", clients[i]->socket, message);
                }
            } else {
                printf("Failed to encode WebSocket message for client %d\n", clients[i]->socket);
            }
        }
    }

    printf("Exiting websocket_broadcast\n");
    pthread_mutex_unlock(&clients_mutex); // 뮤텍스 잠금 해제
}


// WebSocket 키 추출 함수
char *extract_websocket_key(const char *buffer) {
    static char key[256];
    char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) return NULL;
    key_start += strlen("Sec-WebSocket-Key: ");

    // 키 종료 위치를 찾고 추출
    char *key_end = strstr(key_start, "\r\n");
    if (!key_end) return NULL;
    size_t key_length = key_end - key_start;

    strncpy(key, key_start, key_length);
    key[key_length] = '\0';
    return key;
}
// Base64 인코딩 함수
void base64_encode(const unsigned char *input, size_t length, char *output, size_t output_size) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, j;
    for (i = 0, j = 0; i + 2 < length; i += 3) {
        output[j++] = table[(input[i] >> 2) & 0x3F];
        output[j++] = table[((input[i] & 0x03) << 4) | ((input[i + 1] >> 4) & 0x0F)];
        output[j++] = table[((input[i + 1] & 0x0F) << 2) | ((input[i + 2] >> 6) & 0x03)];
        output[j++] = table[input[i + 2] & 0x3F];
    }
    if (i < length) {
        output[j++] = table[(input[i] >> 2) & 0x3F];
        if (i + 1 < length) {
            output[j++] = table[((input[i] & 0x03) << 4) | ((input[i + 1] >> 4) & 0x0F)];
            output[j++] = table[(input[i + 1] & 0x0F) << 2];
        } else {
            output[j++] = table[(input[i] & 0x03) << 4];
        }
        while (j % 4) output[j++] = '=';
    }
    output[j] = '\0';
}
