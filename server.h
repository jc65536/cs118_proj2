#ifndef SERVER_H
#define SERVER_H

#include <stdatomic.h>

#include "common.h"

#define RECVQ_CAPACITY 256
#define ACKQ_CAPACITY 256

struct recvq {
    atomic_size_t rwnd;
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t ack_next;
    struct {
        size_t payload_size;
        bool filled;
        struct packet packet;
    } *buf;
};

struct ackq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    struct {
        size_t packet_size;
        struct packet packet;
    } *buf;
};

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

static void debug_recvq(char *str, struct packet *p, struct recvq *q) {
    printf("%s\tseq %7d\trwnd %3ld\tbegin %3ld\tend %3ld\t\tack_next %3ld\n",
           str, p->seqnum, q->rwnd, q->begin, q->end, q->ack_next);
}

static void debug_ackq(char *str, struct packet *p, struct ackq *q) {
    printf("%s\tseq %7d\tqueued %3ld\tbegin %3ld\tend %3ld\n",
           str, p->seqnum, q->num_queued, q->begin, q->end);
}

#endif
