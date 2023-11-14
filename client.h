#ifndef CLIENT_H
#define CLIENT_H

#include <stdatomic.h>

#include "common.h"

#define SENDQ_CAPACITY 256
#define RETRANSQ_CAPACITY 256

static inline void print_send(const struct packet *pkt, bool resend) {
    if (resend)
        printf("Resend\tseq %7d\t%s\n", pkt->seqnum, is_final(pkt) ? "LAST" : "");
    else
        printf("Send\tseq %7d\t%s\n", pkt->seqnum, is_final(pkt) ? "LAST" : "");
}

struct sendq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t send_next;
    atomic_size_t cwnd;
    struct {
        size_t packet_size;
        struct packet packet;
    } *buf;
};

struct retransq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
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

struct receiver_args {
    struct sendq *sendq;
    struct retransq *retransq;
};

void *read_file(struct reader_args *args);

void *send_packets(struct sender_args *args);

void *receive_acks(struct receiver_args *args);

#endif
