#ifndef CLIENT_H
#define CLIENT_H

#include <stdatomic.h>
#include <threads.h>

#include "common.h"

#define SENDQ_SIZE 256

static inline void print_send(const struct packet *pkt, bool resend) {
    if (resend)
        printf("RESEND %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
    else
        printf("SEND %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
}

enum qstate { EMPTY, FULL, NONEMPTY };

struct sendq {
    mtx_t mutex;
    _Atomic enum qstate state;
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t send_next;
    atomic_size_t cwnd;
    struct packet *buf;
};

// Thread routines

struct reader_args {
    struct sendq *queue;
    const char *filename;
};

void *read_file(struct reader_args *args);

void *send_packets(void *arg);

void *receive_acks(void *arg);

#endif
