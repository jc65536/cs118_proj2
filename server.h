#ifndef SERVER_H
#define SERVER_H

#include <stdatomic.h>

#include "common.h"

#define RECVQ_CAPACITY 256
#define ACKQ_CAPACITY 256

struct recvq;

enum recv_type {
    SEQ,
    RET,
    OOO,
    ERR,
    IGN
};

struct recvq *recvq_new();
enum recv_type recvq_write_slot(struct recvq *q, struct packet *p, size_t payload_size);
bool recvq_pop(struct recvq *q, void (*cont)(const struct packet *, size_t));

struct ackq;

struct ackq *ackq_new();
bool ackq_push(struct ackq *q, struct recvq *recvq, bool nack);
bool ackq_pop(struct ackq *q, void (*cont)(const struct packet *, size_t));

// Thread routines

struct receiver_args {
    struct recvq *recvq;
    struct ackq *ackq;
};

struct writer_args {
    struct recvq *recvq;
};

struct sender_args {
    struct ackq *ackq;
};

void *receive_packets(struct receiver_args *args);
void *write_file(struct writer_args *args);
void *send_acks(struct sender_args *args);

// Debug utils

void debug_recvq(char *str, const struct recvq *q);
void debug_ackq(char *str, const struct ackq *q);

#endif
