#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "client.h"

struct sendq {
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t send_next;
    atomic_size_t num_queued;
    atomic_size_t cwnd;
    atomic_size_t ssthresh;
    atomic_size_t dupACKs;
    atomic_size_t state; // 0=slow start, 1=congestion avoidance, 2=FR
    atomic_size_t in_flight;
    uint32_t seqnum;
    size_t bytes_written;
    struct sendq_slot *slot;

    struct sendq_slot {
        struct packet packet;
        size_t packet_size;
    } *buf;
};

struct sendq *sendq_new() {
    struct sendq *q = calloc(1, sizeof(struct sendq));
    q->cwnd = 1;
    q->ssthresh = 6;
    q->dupACKs = 0;
    q->state = 0;
    q->buf = calloc(SENDQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

struct sendq_slot *sendq_get_slot(const struct sendq *q, size_t i) {
    return &q->buf[i % SENDQ_CAPACITY];
}

void update_cwnd(struct sendq *q, size_t val) {
    q->cwnd = val;
    if (q->cwnd >= q->ssthresh && q->state == 0) {
        q->state = 1; // move to congestion control
    }
}

void update_ssthresh(struct sendq *q, size_t val) {
    q->ssthresh = val;
}

// if in slow start, inc cwnd by 1
// else if in congestion avoidance, inc cwnd by 1/cwnd
// else if in fast recovery, cwnd = ssthresh
void handle_new_ACK(struct sendq *q) {
    static float f = 0;
    q->dupACKs = 0;
    if (q->state == 0) {
        q->cwnd += 1;
        if (q->cwnd >= q->ssthresh) {
            q->state = 1;
        }
    } else if (q->state == 1) {
        f += 1.0 / q->cwnd;
        if (f >= 1) {
            q->cwnd++;
            f = 0;
        }
    } else if (q->state == 2) {
        q->cwnd = q->ssthresh;
        q->state = 1;
    }
}

// if in FR, inc cwnd by 1
// else inc dupACKs and check if it's == 3
// if 3 dupACKs and not in FR, return true
bool handle_dup_ACK(struct sendq *q) {
    if (q->state == 2) {
        q->cwnd += 1;
        return false;
    } else {
        q->dupACKs++;
        if (q->dupACKs == 3) {
            q->state = 2;
            q->ssthresh = q->cwnd / 2;
            q->cwnd = q->ssthresh + 3;
            return true;
        }
    }
    return false;
}

// update sendq values for timeout
void handle_timeout(struct sendq *q) {
    q->state = 0;
    q->ssthresh = q->cwnd / 2;
    q->cwnd = 1;
    q->dupACKs = 0;
}

atomic_size_t get_cwnd(struct sendq *q) {
    return q->cwnd;
}

void sendq_fill_end(struct sendq *q, const char *src, size_t size) {
    while (!q->slot) {
        if (q->num_queued < SENDQ_CAPACITY) {
            q->slot = sendq_get_slot(q, q->end);
            q->slot->packet_size = HEADER_SIZE;
            q->slot->packet.seqnum = q->seqnum;
            q->bytes_written = 0;
        }
    }

    if (q->bytes_written + size < MAX_PAYLOAD_SIZE) {
        memcpy(q->slot->packet.payload + q->bytes_written, src, size);
        q->bytes_written += size;
        q->slot->packet_size += size;
        q->seqnum += size;
    } else {
        size_t rem_capacity = MAX_PAYLOAD_SIZE - q->bytes_written;
        memcpy(q->slot->packet.payload + q->bytes_written, src, rem_capacity);
        q->slot->packet_size = sizeof(struct packet);
        q->seqnum += rem_capacity;

        sendq_flush_end(q, false);

        if (size > rem_capacity)
            sendq_fill_end(q, src + rem_capacity, size - rem_capacity);
    }
}

bool sendq_flush_end(struct sendq *q, bool final) {
    if (!q->slot)
        return false;

    if (final)
        q->slot->packet.flags = FLAG_FINAL;

    q->slot = NULL;
    q->end++;
    q->num_queued++;
    return true;
}

size_t sendq_pop(struct sendq *q, uint32_t acknum) {
    // Round up
    size_t ack_index = (acknum - 1) / MAX_PAYLOAD_SIZE + 1;

    if (ack_index <= q->begin || q->send_next < ack_index) {
        return q->in_flight;
    }

    size_t num_popped = ack_index - q->begin;
    q->begin = ack_index;
    q->in_flight -= num_popped;
    q->num_queued -= num_popped;

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

    return true;
}

bool sendq_lookup_seqnum(const struct sendq *q, uint32_t seqnum,
                         bool (*cont)(const struct packet *, size_t)) {
    size_t index = seqnum / MAX_PAYLOAD_SIZE;

    if (index * MAX_PAYLOAD_SIZE < seqnum)
        index++;

    if (index < q->begin || q->end <= index)
        return false;

    const struct sendq_slot *slot = sendq_get_slot(q, index);
    return cont(&slot->packet, slot->packet_size);
}

const struct packet *sendq_oldest_packet(const struct sendq *q) {
    if (q->in_flight == 0)
        return NULL;
    else
        return &sendq_get_slot(q, q->begin)->packet;
}

struct retransq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    uint32_t buf[RETRANSQ_CAPACITY];
};

struct retransq *retransq_new() {
    return calloc(1, sizeof(struct retransq));
}

bool retransq_push(struct retransq *q, uint32_t seqnum) {
    if (q->num_queued == RETRANSQ_CAPACITY) {
        return false;
    }

    q->buf[q->end % RETRANSQ_CAPACITY] = seqnum;
    q->end++;
    q->num_queued++;

    return true;
}

bool retransq_pop(struct retransq *q, bool (*cont)(uint32_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    uint32_t seqnum = q->buf[q->begin % RETRANSQ_CAPACITY];

    if (!cont(seqnum))
        return false;

    q->begin++;
    q->num_queued--;

    return true;
}
