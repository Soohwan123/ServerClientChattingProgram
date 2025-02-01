#include "websocket.h"

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

// WebSocket 프레임 디코딩
size_t decode_websocket_frame(const char *frame, size_t length, char *decoded_message, size_t buffer_size) {
    if (length < 2) return 0; // 최소 길이 확인

    size_t payload_length = frame[1] & 0x7F; // 첫 7비트에서 길이 추출
    size_t offset = 2;

    if (payload_length == 126) { // 확장길이 (2바이트)
        if (length < 4) return 0;
        payload_length = (frame[2] << 8) | frame[3];
        offset += 2;
    } else if (payload_length == 127) {
        return 0;
    }

    if (length < offset + 4) return 0;

    // 클라이언트 -> 서버 통신에 필요
    const uint8_t *masking_key = (uint8_t *)&frame[offset];
    offset += 4;


    if (length < offset + payload_length) return 0;
    const uint8_t *payload = (uint8_t *)&frame[offset];

    // 페이로드 데이터 - 마스킹 해제
    if (payload_length > buffer_size - 1) return 0;

    for (size_t i = 0; i < payload_length; i++) {
        decoded_message[i] = payload[i] ^ masking_key[i % 4];
    }
    decoded_message[payload_length] = '\0';

    return payload_length;
}

// WebSocket 프레임 인코딩
size_t encode_websocket_frame(const char *message, char *frame, size_t buffer_size) {
    size_t message_length = strlen(message);
    if (message_length > buffer_size - 2) return 0;

    frame[0] = 0x81;    //FIN 플래그 + 텍스트 프레임 (opcode 0x1)
    frame[1] = message_length;
    memcpy(&frame[2], message, message_length); //실제 메모리 복사

    return message_length + 2;
}

// WebSocket 메시지 브로드캐스트
void websocket_broadcast(const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket != sender_socket) {
            char frame[BUFFER_SIZE];
            size_t frame_length = encode_websocket_frame(message, frame, sizeof(frame));
            if (frame_length > 0) {
                send(clients[i]->socket, frame, frame_length, 0);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// WebSocket 키 추출 (Sec-WebSocket-Key)
char *extract_websocket_key(const char *buffer) {
    static char key[256];
    char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) return NULL;
    key_start += strlen("Sec-WebSocket-Key: ");

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