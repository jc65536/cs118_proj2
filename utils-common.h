#ifndef UTILS_COMMON_H
#define UTILS_COMMON_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MACROS
#define SERVER_IP "127.0.0.1"
#define LOCAL_HOST "127.0.0.1"
#define SERVER_PORT_TO 5002
#define CLIENT_PORT 6001
#define SERVER_PORT 6002
#define CLIENT_PORT_TO 5001
#define MAX_PAYLOAD_SIZE 1024
#define WINDOW_SIZE 5
#define TIMEOUT 2
#define MAX_SEQUENCE 1024

#define FLAG_FINAL 0b00000001

struct packet {
    uint32_t seqnum;
    uint16_t recv_window;
    uint8_t flags;
    char payload[];
};

extern inline bool is_last(const struct packet *p) {
    return p->flags & FLAG_FINAL;
}

#endif
