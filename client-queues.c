#include <stdatomic.h>

#include "client.h"

struct sendq {
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t send_next;
    atomic_size_t num_queued;
    atomic_size_t cwnd;
    atomic_size_t in_flight;
    struct sendq_slot {
        struct packet packet;
        size_t packet_size;
    } *buf;
};

struct sendq *sendq_new() {
    struct sendq *q = calloc(1, sizeof(struct sendq));
    q->cwnd = 16;
    q->buf = calloc(SENDQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

bool sendq_write(struct sendq *q, bool (*cont)(struct packet *, size_t *)) {
    if (q->num_queued == SENDQ_CAPACITY) {
        return false;
    }

    struct sendq_slot *slot = &q->buf[q->end % SENDQ_CAPACITY];

    if (!cont(&slot->packet, &slot->packet_size))
        return false;

    q->end++;
    q->num_queued++;

    debug_sendq("Read", q);
    return true;
}

size_t sendq_pop(struct sendq *q, uint32_t acknum) {
    // Round up
    size_t ack_index = (acknum - 1) / MAX_PAYLOAD_SIZE + 1;

    if (ack_index <= q->begin || q->send_next < ack_index) {
        debug_sendq(format("sendq can't pop %ld", ack_index), q);
        return q->in_flight;
    }

    size_t num_popped = ack_index - q->begin;
    q->begin = ack_index;
    q->in_flight -= num_popped;
    q->num_queued -= num_popped;

    debug_sendq(format("Received ack %d", acknum), q);
    return q->in_flight;
}

const struct sendq_slot *sendq_get_slot(const struct sendq *q, size_t i) {
    return &q->buf[i % SENDQ_CAPACITY];
}

bool sendq_send_next(struct sendq *q, bool (*cont)(const struct packet *, size_t)) {
    if (q->send_next == q->begin + q->cwnd || q->send_next == q->end) {
        return false;
    }

    const struct sendq_slot *slot = sendq_get_slot(q, q->send_next);

    if (!cont(&slot->packet, slot->packet_size))
        return false;

    q->send_next++;
    q->in_flight++;

    debug_sendq("Send", q);

    return true;
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
    size_t buf[RETRANSQ_CAPACITY];
};

struct retransq *retransq_new() {
    return calloc(1, sizeof(struct retransq));
}

bool retransq_push(struct retransq *q, uint32_t seqnum) {
    if (q->num_queued == RETRANSQ_CAPACITY) {
        return false;
    }

    size_t index = seqnum / MAX_PAYLOAD_SIZE;
    q->buf[q->end % RETRANSQ_CAPACITY] = index;

    q->end++;
    q->num_queued++;

    debug_retransq(format("Queued retransmit %ld", index), q);

    return true;
}

bool retransq_pop(struct retransq *q, const struct sendq *sendq,
                  bool (*cont)(const struct packet *, size_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    size_t index = q->buf[q->begin % RETRANSQ_CAPACITY];
    const struct sendq_slot *slot = sendq_get_slot(sendq, index);

    if (!cont(&slot->packet, slot->packet_size))
        return false;

    q->begin++;
    q->num_queued--;

    debug_retransq(format("Retransmitted %ld", index), q);

    return true;
}

void debug_sendq(const char *str, const struct sendq *q) {
    printf("%-32s  begin %6ld  end %6ld  send_next %6ld  num_queued %6ld  in_flight %6ld\n",
           str, q->begin, q->end, q->send_next, q->num_queued, q->in_flight);
}

void debug_retransq(const char *str, const struct retransq *q) {
    printf("%-32s  begin %6ld  end %6ld  num_queued %6ld\n",
           str, q->begin, q->end, q->num_queued);
}
