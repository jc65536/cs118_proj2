#ifndef CLIENT_H
#define CLIENT_H

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

/* If possible, call write to write a packet into q. Returns whether the write
 * was successful.
 */
bool sendq_write(struct sendq *q, bool (*write)(struct packet *, size_t *));

/* If possible, pop packets from q until acknum. Returns the remaining number of
 * in-flight packets (number of packets sent but not ACKed).
 */
size_t sendq_pop(struct sendq *q, uint32_t acknum);

/* If possible, call cont to send the next packet. Returns whether the send was
 * successful.
 */
bool sendq_send_next(struct sendq *q, bool (*cont)(const struct packet *, size_t));

/* If possible, pass the packet specified by seqnum to cont. Returns whether the
 * seqnum was valid and cont was successful.
 */
bool sendq_lookup_seqnum(const struct sendq *q, uint32_t seqnum,
                         bool (*cont)(const struct packet *, size_t));

void sendq_fill_end(struct sendq *q, const char *src, size_t size);
bool sendq_flush_end(struct sendq *q, bool final);

/* Returns the oldest in-flight packet or NULL if there are no in-flight packets.
 */
const struct packet *sendq_oldest_packet(const struct sendq *q);

size_t sendq_get_cwnd(struct sendq *q);
void sendq_inc_cwnd(struct sendq *q);
void sendq_halve_cwnd(struct sendq *q);

struct retransq;

struct retransq *retransq_new();

/* If possible, push seqnum onto q. Returns whether the push was successful.
 */
bool retransq_push(struct retransq *q, uint32_t seqnum);

/* If possible, call cont to process the next seqnum, then pop q. Returns whether
 * a seqnum was popped and cont was successful.
 */
bool retransq_pop(struct retransq *q, bool (*cont)(uint32_t));

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

void *read_and_compress(struct reader_args *args);
void *send_packets(struct sender_args *args);
void *receive_acks(struct receiver_args *args);

// Debug

void debug_sendq(const char *str, const struct sendq *q);
void debug_retransq(const char *str, const struct retransq *q);

#endif
