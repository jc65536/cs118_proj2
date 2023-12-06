#ifndef COMMON_H
#define COMMON_H
#include <stdbool.h>
#include <stddef.h>
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
#define MAX_PAYLOAD_SIZE 1195
#define WINDOW_SIZE 5
#define MAX_SEQUENCE 1024

#define NUM_THREADS 4

#define FLAG_FINAL 0b00000001

#ifdef DEBUG
#define DBG(x) (x)
#else
#define DBG(x) \
    do {       \
    } while (0)
#endif

typedef void *(*voidfn)(void *);

typedef uint32_t seqnum_t;

struct packet {
    seqnum_t seqnum;
    uint8_t flags;
    char payload[MAX_PAYLOAD_SIZE];
};

#define HEADER_SIZE (offsetof(struct packet, payload))

bool is_final(const struct packet *p);

#ifdef DEBUG
const char *format(const char *fmt, ...);
#endif

#endif
