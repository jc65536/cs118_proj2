#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "client.h"
#include "rto.h"

struct sendq {
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t send_next;
    atomic_size_t num_queued;
    atomic_size_t cwnd;
    atomic_size_t in_flight;
    size_t bytes_written;
    struct sendq_slot *slot;
    uint32_t ssthresh;

    struct sendq_slot {
        struct packet packet;
        size_t packet_size;
        bool sacked;
    } *buf;
};

struct sendq *sendq_new() {
    struct sendq *q = calloc(1, sizeof(struct sendq));
    q->cwnd = 1;
    q->ssthresh = 144;
    q->buf = calloc(SENDQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

uint32_t sendq_halve_ssthresh(struct sendq *q) {
    uint32_t ssthresh = q->in_flight / 2;
    if (ssthresh < 2)
        ssthresh = 2;
    return q->ssthresh = ssthresh;
}

uint32_t sendq_get_ssthresh(struct sendq *q) {
    return q->ssthresh;
}

size_t sendq_get_cwnd(struct sendq *q) {
    return q->cwnd;
}

void sendq_set_cwnd(struct sendq *q, size_t cwnd) {
    q->cwnd = cwnd;
}

size_t sendq_inc_cwnd(struct sendq *q) {
    return q->cwnd += 1;
}

struct sendq_slot *sendq_get_slot(const struct sendq *q, size_t i) {
    return &q->buf[i % SENDQ_CAPACITY];
}

bool sendq_write(struct sendq *q, bool (*write)(struct packet *, size_t *)) {
    if (q->num_queued == SENDQ_CAPACITY) {
        return false;
    }

    struct sendq_slot *slot = sendq_get_slot(q, q->end);
    slot->sacked = false;

    if (!write(&slot->packet, &slot->packet_size))
        return false;

    q->end++;
    q->num_queued++;

    DBG(debug_sendq("Read", q));
    return true;
}

size_t sendq_pop(struct sendq *q, seqnum_t acknum) {
    // Round up
    if (acknum <= q->begin || q->send_next < acknum) {
        DBG(debug_sendq(format("sendq can't pop %d", acknum), q));
        return q->in_flight;
    }

    while (acknum < q->send_next && sendq_get_slot(q, acknum)->sacked)
        acknum++;

    size_t num_popped = acknum - q->begin;

    q->begin = acknum;
    q->in_flight -= num_popped;
    q->num_queued -= num_popped;

    DBG(debug_sendq(format("Received ack %d", acknum), q));
    return q->in_flight;
}

bool sendq_send_next(struct sendq *q, bool (*cont)(const struct packet *, size_t)) {
    if (q->send_next >= q->begin + q->cwnd || q->send_next == q->end) {
        return false;
    }

    const struct sendq_slot *slot = sendq_get_slot(q, q->send_next);

    if (!cont(&slot->packet, slot->packet_size))
        return false;

    q->send_next++;
    q->in_flight++;

    DBG(debug_sendq(format("Send %d", slot->packet.seqnum), q));
    return true;
}

bool sendq_lookup_seqnum(const struct sendq *q, seqnum_t seqnum,
                         bool (*cont)(const struct packet *, size_t)) {
    if (seqnum < q->begin || q->end <= seqnum)
        return false;

    const struct sendq_slot *slot = sendq_get_slot(q, seqnum);
    return cont(&slot->packet, slot->packet_size);
}

const struct packet *sendq_oldest_packet(const struct sendq *q) {
    if (q->in_flight == 0)
        return NULL;
    else
        return &sendq_get_slot(q, q->begin)->packet;
}

void sendq_sack(struct sendq *q, seqnum_t start, const seqnum_t *holes, size_t holes_len) {
    if (start < q->begin || holes_len == 0)
        return;
    
    for (seqnum_t i = start + 1; i < *holes; i++)
        sendq_get_slot(q, i)->sacked = true;

    sendq_sack(q, *holes, holes + 1, holes_len - 1);
}

struct retransq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    seqnum_t buf[RETRANSQ_CAPACITY];
};

struct retransq *retransq_new() {
    return calloc(1, sizeof(struct retransq));
}

bool retransq_push(struct retransq *q, seqnum_t seqnum) {
    if (q->num_queued == RETRANSQ_CAPACITY) {
        return false;
    }

    q->buf[q->end % RETRANSQ_CAPACITY] = seqnum;
    q->end++;
    q->num_queued++;

    DBG(debug_retransq(format("Queued retransmit %d", seqnum), q));
    return true;
}

bool retransq_pop(struct retransq *q, bool (*cont)(seqnum_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    seqnum_t seqnum = q->buf[q->begin % RETRANSQ_CAPACITY];

    if (!cont(seqnum))
        return false;

    q->begin++;
    q->num_queued--;

    DBG(debug_retransq(format("Retransmitted %d", seqnum), q));
    return true;
}

#ifdef DEBUG
void debug_sendq(const char *str, const struct sendq *q) {
    printf("[sendq] %-32s  begin %6ld  end %6ld  send_next %6ld  num_queued %6ld  in_flight %6ld  cwnd %6ld\n",
           str, q->begin, q->end, q->send_next, q->num_queued, q->in_flight, q->cwnd);
}
#endif

#ifdef DEBUG
void debug_retransq(const char *str, const struct retransq *q) {
    printf("[retransq] %-32s  begin %6ld  end %6ld  num_queued %6ld\n",
           str, q->begin, q->end, q->num_queued);
}
#endif

#ifdef DEBUG
void profile(union sigval args) {
    static int fd;
    static char str[256];
    static size_t str_size;

    if (!fd) {
        fd = creat("ignore/client-bufs.csv", S_IRUSR | S_IWUSR);
        str_size = sprintf(str, "in_flight,read,retrans,cwnd,ssthresh,rto\n");
        write(fd, str, str_size);
    }

    struct profiler_args *pargs = (struct profiler_args *) args.sival_ptr;
    const struct sendq *sendq = pargs->sendq;
    const struct retransq *retransq = pargs->retransq;

    size_t in_flight = sendq->in_flight;
    size_t read = sendq->num_queued - in_flight;
    size_t retrans = retransq->num_queued;
    size_t cwnd = sendq->cwnd;
    uint32_t ssthresh = sendq->ssthresh;
    double rto_ = rto.tv_sec + rto.tv_nsec / (double) S_TO_NS;
    str_size = sprintf(str, "%ld,%ld,%ld,%ld,%d,%f\n", in_flight, read, retrans, cwnd, ssthresh, rto_);
    write(fd, str, str_size);
}
#endif
