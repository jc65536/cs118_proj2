#ifndef SERVER_H
#define SERVER_H

#include <stdatomic.h>
#include <time.h>
#include <signal.h>

#include "common.h"

#define RECVQ_CAPACITY 256
#define RECVBUF_CAPACITY 256
#define ACKQ_CAPACITY 256

struct recvq;

struct recvq *recvq_new();
bool recvq_write(struct recvq *q, void (*write)(struct packet *, size_t *));
bool recvq_pop(struct recvq *q, void (*cont)(const struct packet *, size_t));

struct recvbuf;

struct recvbuf *recvbuf_new();
void recvbuf_push(struct recvbuf *b, const struct packet *p, size_t payload_size);
uint16_t recvbuf_get_rwnd(const struct recvbuf *b);
uint32_t recvbuf_get_acknum(const struct recvbuf *b);
size_t recvbuf_take_begin(struct recvbuf *b, char *dest, size_t size);

struct ackq;

struct ackq *ackq_new();
bool ackq_push(struct ackq *q, uint32_t acknum);
bool ackq_pop(struct ackq *q, bool (*cont)(uint32_t));

// Thread routines

struct receiver_args {
    struct recvq *recvq;
};

struct copier_args {
    struct recvq *recvq;
    struct recvbuf *recvbuf;
    struct ackq *ackq;
};

struct writer_args {
    struct recvbuf *recvbuf;
};

struct sender_args {
    struct ackq *ackq;
    struct recvbuf *recvbuf;
};

void *receive_packets(struct receiver_args *args);
void *copy_packets(struct copier_args *args);
void *decompress_and_write(struct writer_args *args);
void *send_acks(struct sender_args *args);

#endif
