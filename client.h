#ifndef CLIENT_H
#define CLIENT_H

#include <signal.h>
#include <time.h>

#include "common.h"

#define SENDQ_CAPACITY 512
#define RETRANSQ_CAPACITY 256

struct sendq;
struct retransq;

extern volatile seqnum_t *holes;
extern volatile size_t holes_len;

enum trans_state {
    SLOW_START,
    CONGESTION_AVOIDANCE,
    FAST_RECOVERY
};

extern volatile enum trans_state state;

struct sendq *sendq_new();

/* If possible, call write to write a packet into q. Returns whether the write
 * was successful.
 */
bool sendq_write(struct sendq *q, bool (*write)(struct packet *, size_t *));

/* If possible, pop packets from q until acknum. Returns the number of packets
 * popped.
 */
size_t sendq_pop(struct sendq *q, seqnum_t acknum);

/* If possible, call cont to send the next packet. Returns whether the send was
 * successful.
 */
bool sendq_send_next(struct sendq *q, bool (*cont)(const struct packet *, size_t));

void sendq_sack(struct sendq *q, const volatile seqnum_t *hills, size_t hills_len);

size_t sendq_get_in_flight(const struct sendq *q);

void sendq_retrans_holes(const struct sendq *q, struct retransq *retransq);

uint32_t sendq_get_ssthresh(const struct sendq *q);
size_t sendq_get_cwnd(const struct sendq *q);
void sendq_set_cwnd(struct sendq *q, size_t cwnd);
size_t sendq_inc_cwnd(struct sendq *q);
uint32_t sendq_halve_ssthresh(struct sendq *q);

struct retransq *retransq_new();

/* If possible, call cont to process the next seqnum, then pop q. Returns whether
 * a seqnum was popped and cont was successful.
 */
bool retransq_pop(struct retransq *q, bool (*cont)(const struct packet *, size_t));

extern volatile bool timer_set;

// Thread routines

struct timer_args {
    struct sendq *sendq;
    struct retransq *retransq;
    timer_t timer;
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

struct profiler_args {
    const struct sendq *sendq;
    const struct retransq *retransq;
};

void handle_timer(union sigval args);
void set_timer(timer_t t);
void unset_timer(timer_t t);

void *read_file(struct reader_args *args);
void *send_packets(struct sender_args *args);
void *receive_acks(struct receiver_args *args);

// Debug

#ifdef DEBUG
void debug_sendq(const char *str, const struct sendq *q);
void debug_retransq(const char *str, const struct retransq *q);
void profile(union sigval args);
#endif

#endif
