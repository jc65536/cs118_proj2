#ifndef SERVER_H
#define SERVER_H

#include <stdatomic.h>

#include "common.h"

#define RECVQ_CAPACITY 256
#define RECVBUF_CAPACITY 256
#define ACKQ_CAPACITY 256

struct recvq;

struct recvq *recvq_new();
bool recvq_write(struct recvq *q, void (*cont)(struct packet *, size_t *));
bool recvq_pop(struct recvq *q, void (*cont)(const struct packet *, size_t));

struct recvbuf;

enum recv_type {
    OK,
    ERR,
    END
};

struct recvbuf *recvbuf_new();
enum recv_type recvbuf_write(struct recvbuf *q, const struct packet *p, size_t payload_size);
bool recvbuf_pop(struct recvbuf *q, void (*cont)(const struct packet *, size_t));

struct ackq;

struct ackq *ackq_new();
bool ackq_push(struct ackq *q, const struct recvbuf *recvbuf);
bool ackq_pop(struct ackq *q, void (*cont)(const struct packet *, size_t));

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
};

void *receive_packets(struct receiver_args *args);
void *copy_packets(struct copier_args *args);
void *write_file(struct writer_args *args);
void *send_acks(struct sender_args *args);

// Debug utils

void debug_recvq(const char *str, const struct recvbuf *q);
void debug_ackq(const char *str, const struct ackq *q);

#endif
