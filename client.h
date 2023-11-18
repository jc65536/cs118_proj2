#ifndef CLIENT_H
#define CLIENT_H

#include <stdatomic.h>
#include <signal.h>
#include <time.h>

#include "common.h"

#define SENDQ_CAPACITY 256
#define RETRANSQ_CAPACITY 256

static inline void print_send(const struct packet *pkt, bool resend) {
    if (resend)
        printf("Resend\tseq %7d\t%s\n", pkt->seqnum, is_final(pkt) ? "LAST" : "");
    else
        printf("Send\tseq %7d\t%s\n", pkt->seqnum, is_final(pkt) ? "LAST" : "");
}

struct sendq;

struct sendq *sendq_new();
bool sendq_write(struct sendq *q, bool (*cont)(struct packet *, size_t *));
// Returns q->in_flight afterwards
size_t sendq_pop(struct sendq *q, uint32_t acknum);
bool sendq_consume_next(struct sendq *q, bool (*cont)(const struct packet *, size_t));
const struct packet *sendq_oldest_packet(const struct sendq *q);

struct retransq;

struct retransq *retransq_new();
bool retransq_push(struct retransq *q, const uint32_t seqnums);
bool retransq_pop(struct retransq *q, const struct sendq *sendq,
                  bool (*cont)(const struct packet *, size_t));

// Thread routines

struct timer_args {
    struct sendq *sendq;
    struct retransq *retransq;
};

struct reader_args {
    struct sendq *sendq;
    const char *filename;
};

struct sender_args {
    struct sendq *sendq;
    struct retransq *retransq;
    timer_t timer;
};

struct receiver_args {
    struct sendq *sendq;
    struct retransq *retransq;
    timer_t timer;
};

void handle_timer(union sigval args);
void set_timer(timer_t t);
void unset_timer(timer_t t);
bool is_timer_set(timer_t t);

void *read_file(struct reader_args *args);
void *send_packets(struct sender_args *args);
void *receive_acks(struct receiver_args *args);

// Debug

void debug_sendq(char *str, const struct sendq *q);
void debug_retransq(char *str, const struct retransq *q);

#endif
