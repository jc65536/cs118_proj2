#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include <stdatomic.h>
#include <time.h>

#include "common.h"

#define RECVQ_CAPACITY 64
#define RECVBUF_CAPACITY 512
#define ACKQ_CAPACITY 256

struct recvq;

struct recvq *recvq_new();
bool recvq_write(struct recvq *q, void (*write)(struct packet *, size_t *));
bool recvq_pop(struct recvq *q, void (*cont)(const struct packet *, size_t));

struct recvbuf;

enum recv_type {
    OK,
    ERR,
    END
};

struct recvbuf *recvbuf_new();
enum recv_type recvbuf_push(struct recvbuf *b, const struct packet *p, size_t payload_size);
bool recvbuf_pop(struct recvbuf *b, bool (*cont)(const struct packet *, size_t));
uint32_t recvbuf_get_acknum(const struct recvbuf *b);
size_t recvbuf_take_begin(struct recvbuf *b, char *dest, size_t size);
size_t recvbuf_write_holes(struct recvbuf *b, char *dest, size_t size);

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

struct profiler_args {
    const struct recvq *recvq;
    const struct recvbuf *recvbuf;
    const struct ackq *ackq;
};

void *receive_packets(struct receiver_args *args);
void *copy_packets(struct copier_args *args);
void *decompress_and_write(struct writer_args *args);
void *send_acks(struct sender_args *args);

void profile(union sigval args);

// Debug utils

void debug_recvbuf(const char *str, const struct recvbuf *q);
void debug_ackq(const char *str, const struct ackq *q);

#endif
