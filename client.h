#ifndef CLIENT_H
#define CLIENT_H

#include <stdatomic.h>

#include "common.h"

#define SENDQ_CAPACITY 256
#define RETRANSQ_CAPACITY 128

static inline void print_send(const struct packet *pkt, bool resend) {
    if (resend)
        printf("RESEND %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
    else
        printf("SEND %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
}

struct sendq {
    atomic_size_t num_queued;
    size_t begin;
    size_t end;
    size_t send_next;
    size_t cwnd;
    struct packet *buf;
};

struct retransq {
    atomic_size_t num_queued;
    size_t begin;
    size_t end;
    uint32_t buf[RETRANSQ_CAPACITY];
};

// Thread routines

struct reader_args {
    struct sendq *sendq;
    const char *filename;
};

struct sender_args {
    struct sendq *sendq;
    struct retransq *retransq;
};

void *read_file(struct reader_args *args);

void *send_packets(struct sender_args *args);

void *receive_acks(void *arg);

#endif
