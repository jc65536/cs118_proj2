#include "server.h"

struct recvq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    struct recvq_slot {
        struct packet packet;
        size_t packet_size;
    } *buf;
};

struct recvq *recvq_new() {
    struct recvq *q = calloc(1, sizeof(struct recvq));
    q->buf = calloc(RECVQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

bool recvq_write(struct recvq *q, void (*write)(struct packet *, size_t *)) {
    if (q->num_queued == RECVQ_CAPACITY) {
        return false;
    }

    struct recvq_slot *slot = &q->buf[q->end % RECVQ_CAPACITY];
    write(&slot->packet, &slot->packet_size);

    q->end++;
    q->num_queued++;
    return true;
}

bool recvq_pop(struct recvq *q, void (*cont)(const struct packet *, size_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    const struct recvq_slot *slot = &q->buf[q->begin % RECVQ_CAPACITY];
    cont(&slot->packet, slot->packet_size);

    q->begin++;
    q->num_queued--;
    return true;
}

struct recvbuf {
    atomic_size_t rwnd;
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t ack_index;
    _Atomic uint32_t acknum;
    struct recv_slot {
        struct packet packet;
        size_t payload_size;
        bool filled;
    } *buf;
};

struct recvbuf *recvbuf_new() {
    struct recvbuf *b = calloc(1, sizeof(struct recvbuf));
    b->rwnd = RECVBUF_CAPACITY;
    b->buf = calloc(RECVBUF_CAPACITY, sizeof(b->buf[0]));
    return b;
}

struct recv_slot *recvbuf_get_slot(const struct recvbuf *b, size_t i) {
    return b->buf + i % RECVBUF_CAPACITY;
}

enum recv_type recvbuf_push(struct recvbuf *b, const struct packet *p, size_t payload_size) {
    size_t packet_index = p->seqnum / MAX_PAYLOAD_SIZE;

    if (packet_index < b->ack_index || b->end + b->rwnd <= packet_index) {
        debug_recvbuf(format("Out of bounds %ld", packet_index), b);
        return ERR;
    }

    struct recv_slot *slot = recvbuf_get_slot(b, packet_index);

    if (slot->filled) {
        debug_recvbuf(format("Duplicate %ld", packet_index), b);
        return ERR;
    }

    memcpy(&slot->packet, p, HEADER_SIZE + payload_size);
    slot->payload_size = payload_size;
    slot->filled = true;

    enum recv_type ret = OK;

    if (packet_index == b->ack_index) {
        size_t i = b->ack_index;
        uint32_t new_acknum = b->acknum;
        const struct recv_slot *next_slot;

        while ((next_slot = recvbuf_get_slot(b, i))->filled) {
            new_acknum += next_slot->payload_size;
            if (is_final(&next_slot->packet)) {
                ret = END;
            }
            i++;
        }

        b->ack_index = i;
        b->acknum = new_acknum;
    }

    if (b->end <= packet_index) {
        size_t end_inc = packet_index + 1 - b->end;
        b->end += end_inc;
        b->rwnd -= end_inc;
    }

    debug_recvbuf(format("Received %ld", packet_index), b);

    return ret;
}

bool recvbuf_pop(struct recvbuf *b, bool (*cont)(const struct packet *, size_t)) {
    if (b->begin == b->ack_index) {
        return false;
    }

    struct recv_slot *slot = recvbuf_get_slot(b, b->begin);
    slot->filled = false;

    if (!cont(&slot->packet, slot->payload_size))
        return false;

    b->begin++;
    b->rwnd++;

    debug_recvbuf("Wrote", b);
    return true;
}

uint16_t recvbuf_get_rwnd(const struct recvbuf *b) {
    return b->rwnd;
}

uint32_t recvbuf_get_acknum(const struct recvbuf *b) {
    return b->acknum;
}

struct ackq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    uint32_t buf[ACKQ_CAPACITY];
};

struct ackq *ackq_new() {
    return calloc(1, sizeof(struct ackq));
}

bool ackq_push(struct ackq *q, uint32_t acknum) {
    if (q->num_queued == ACKQ_CAPACITY) {
        return false;
    }

    q->buf[q->end % ACKQ_CAPACITY] = acknum;

    q->end++;
    q->num_queued++;

    debug_ackq(format("Queued ack %d", acknum / MAX_PAYLOAD_SIZE), q);
    return true;
}

bool ackq_pop(struct ackq *q, bool (*cont)(uint32_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    uint32_t acknum = q->buf[q->begin % ACKQ_CAPACITY];

    if (!cont(acknum))
        return false;

    q->begin++;
    q->num_queued--;

    debug_ackq(format("Sent ack %d", acknum / MAX_PAYLOAD_SIZE), q);
    return true;
}

// Debug

void debug_recvbuf(const char *str, const struct recvbuf *b) {
    printf("[recvbuf] %-32s  rwnd %6ld  begin %6ld  end %6ld  ack_index %6ld  acknum %8d\n",
           str, b->rwnd, b->begin, b->end, b->ack_index, b->acknum);
}

void debug_ackq(const char *str, const struct ackq *q) {
    printf("[ackq] %-32s  queued %6ld  begin %6ld  end %6ld\n",
           str, q->num_queued, q->begin, q->end);
}
