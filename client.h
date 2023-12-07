#ifndef CLIENT_H
#define CLIENT_H

#include <signal.h>
#include <time.h>

#include "common.h"

#define SENDQ_CAPACITY 512
#define RETRANSQ_CAPACITY 256

struct sendq;
struct retransq;

extern seqnum_t *holes;
extern size_t holes_len;

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

/* If possible, pass the packet specified by seqnum to cont. Returns whether the
 * seqnum was valid and cont was successful.
 */
bool sendq_lookup_seqnum(const struct sendq *q, seqnum_t seqnum,
                         bool (*cont)(const struct packet *, size_t));

void sendq_sack(struct sendq *q, const seqnum_t *hills, size_t hills_len);

size_t sendq_get_in_flight(struct sendq *q);

void sendq_retrans_holes(struct sendq *q, struct retransq *retransq);

/* Returns the oldest in-flight packet or NULL if there are no in-flight packets.
 */
const struct packet *sendq_oldest_packet(const struct sendq *q);

uint32_t sendq_get_ssthresh(struct sendq *q);
size_t sendq_get_cwnd(struct sendq *q);
void sendq_set_cwnd(struct sendq *q, size_t cwnd);
size_t sendq_inc_cwnd(struct sendq *q);
uint32_t sendq_halve_ssthresh(struct sendq *q);

struct retransq *retransq_new();

/* If possible, push seqnum onto q. Returns whether the push was successful.
 */
bool retransq_push(struct retransq *q, seqnum_t seqnum);

/* If possible, call cont to process the next seqnum, then pop q. Returns whether
 * a seqnum was popped and cont was successful.
 */
bool retransq_pop(struct retransq *q, bool (*cont)(seqnum_t));

extern bool timer_set;

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
bool is_timer_set(timer_t t);

void *read_and_compress(struct reader_args *args);
void *send_packets(struct sender_args *args);
void *receive_acks(struct receiver_args *args);

enum trans_state {
    SLOW_START,
    CONGESTION_AVOIDANCE,
    FAST_RECOVERY
};

extern enum trans_state state;

// Debug

#ifdef DEBUG
void debug_sendq(const char *str, const struct sendq *q);
void debug_retransq(const char *str, const struct retransq *q);
void profile(union sigval args);
#endif

#endif
