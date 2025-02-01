#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <stdint.h>
#include "server.h"

// 핸드셰이크 처리
void websocket_handshake(int client_fd, const char *sec_websocket_key);

// 메시지 디코딩 인코딩
size_t decode_websocket_frame(const char *frame, size_t length, char *decoded_message, size_t buffer_size);
size_t encode_websocket_frame(const char *message, char *frame, size_t buffer_size);
int websocket_decode_message(const char *input, char *output, size_t input_len);

// 브로드 캐스트
void websocket_broadcast(const char *message, int sender_socket);


// 웹소켓 키 추출 및 Base64 인코딩
char *extract_websocket_key(const char *buffer);
void base64_encode(const unsigned char *input, size_t length, char *output, size_t output_size);

#endif