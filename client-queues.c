#include "client.h"

struct sendq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t send_next;
    atomic_size_t cwnd;
    struct sendq_slot {
        struct packet packet;
        size_t packet_size;
    } *buf;
};

struct sendq *sendq_new() {
    struct sendq *q = calloc(1, sizeof(struct sendq));
    q->buf = calloc(SENDQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

bool sendq_write(struct sendq *q, void (*cont)(struct packet *, size_t *)) {
    if (q->num_queued == SENDQ_CAPACITY) {
        // debug_sendq("sendq full", q);
        return false;
    }

    struct sendq_slot *slot = &q->buf[q->end % SENDQ_CAPACITY];

    cont(&slot->packet, &slot->packet_size);

    q->end++;
    q->num_queued++;

    debug_sendq("Read", q);
    return true;
}

void sendq_pop(struct sendq *q, uint32_t acknum) {
    if (q->num_queued == 0) {
        return;
    }

    // Round up
    size_t ack_index = (acknum - 1) / MAX_PAYLOAD_SIZE + 1;

    if (ack_index <= q->begin || q->end < ack_index) {
        printf("Can't pop %ld\n", ack_index);
        debug_sendq("ACK Ooo", q);
        return;
    }

    size_t num_popped = ack_index - q->begin;
    q->begin = ack_index;
    q->num_queued -= num_popped;

    debug_sendq("ACK", q);
}

const struct sendq_slot *sendq_get_slot(const struct sendq *q, size_t i) {
    return &q->buf[i % SENDQ_CAPACITY];
}

bool sendq_consume_next(struct sendq *q, void (*cont)(const struct packet *, size_t)) {
    if (q->send_next == q->end) {
        // debug_sendq("No packets to send", q);
        return false;
    }

    const struct sendq_slot *slot = sendq_get_slot(q, q->send_next);
    cont(&slot->packet, slot->packet_size);
    q->send_next++;

    debug_sendq("Send", q);

    return true;
}

const struct packet *sendq_oldest_packet(const struct sendq *q) {
    if (q->num_queued == 0)
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

size_t retransq_push(struct retransq *q, const uint32_t *seqnums, size_t seqnum_count) {
    size_t rem_capacity = RETRANSQ_CAPACITY - q->num_queued;

    if (rem_capacity == 0) {
        // debug_retransq("retransq full", q);
        return 0;
    }

    seqnum_count = seqnum_count < rem_capacity ? seqnum_count : rem_capacity;

    for (size_t i = 0; i < seqnum_count; i++)
        q->buf[(q->end + i) % RETRANSQ_CAPACITY] = seqnums[seqnum_count - 1 - i];

    q->end += seqnum_count;
    q->num_queued += seqnum_count;

    debug_retransq("NACK", q);

    return seqnum_count;
}

bool retransq_pop(struct retransq *q, const struct sendq *sendq,
                  void (*cont)(const struct packet *, size_t)) {
    if (q->num_queued == 0) {
        // debug_retransq("retransq empty", q);
        return false;
    }

    uint32_t seqnum = q->buf[q->begin % RETRANSQ_CAPACITY];
    const struct sendq_slot *slot = sendq_get_slot(sendq, seqnum / MAX_PAYLOAD_SIZE);

    cont(&slot->packet, slot->packet_size);

    q->begin++;
    q->num_queued--;

    debug_retransq("Retrans", q);

    return true;
}

void debug_sendq(char *str, const struct sendq *q) {
    printf("%s\tbegin %3ld\tend %3ld\tsend %3ld\tqueued %3ld\n",
           str, q->begin, q->end, q->send_next, q->num_queued);
}

void debug_retransq(char *str, const struct retransq *q) {
    printf("%s\tbegin %3ld\tend %3ld\tqueued %3ld\n",
           str, q->begin, q->end, q->num_queued);
}
