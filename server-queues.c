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

bool recvq_write(struct recvq *q, void (*cont)(struct packet *, size_t *)) {
    if (q->num_queued == RECVQ_CAPACITY) {
        return false;
    }

    struct recvq_slot *slot = &q->buf[q->end % RECVQ_CAPACITY];
    cont(&slot->packet, &slot->packet_size);

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

enum recv_type recvbuf_write(struct recvbuf *b, const struct packet *p, size_t payload_size) {
    size_t packet_index = p->seqnum / MAX_PAYLOAD_SIZE;

    if (packet_index < b->ack_index || b->end + b->rwnd <= packet_index) {
        debug_recvq(format("Out of bounds %ld", packet_index), b);
        return ERR;
    }

    struct recv_slot *slot = &b->buf[packet_index % RECVBUF_CAPACITY];

    if (slot->filled) {
        debug_recvq(format("Duplicate %ld", packet_index), b);
        return ERR;
    }

    memcpy(&slot->packet, p, sizeof(*p));
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
        size_t rwnd_decrement = packet_index + 1 - b->end;
        b->end = packet_index + 1;
        b->rwnd -= rwnd_decrement;
    }

    debug_recvq(format("Received %ld", packet_index), b);

    return ret;
}

bool recvbuf_pop(struct recvbuf *b, void (*cont)(const struct packet *, size_t)) {
    if (b->begin == b->ack_index) {
        return false;
    }

    struct recv_slot *slot = recvbuf_get_slot(b, b->begin);
    slot->filled = false;

    cont(&slot->packet, slot->payload_size);

    b->begin++;
    b->rwnd++;

    debug_recvq("Wrote", b);
    return true;
}

struct ackq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    struct ack_slot {
        struct packet packet;
        size_t packet_size;
    } *buf;
};

struct ackq *ackq_new() {
    struct ackq *q = calloc(1, sizeof(struct ackq));
    q->buf = calloc(ACKQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

bool ackq_push(struct ackq *q, const struct recvbuf *recvbuf) {
    if (q->num_queued == ACKQ_CAPACITY) {
        return false;
    }

    struct ack_slot *slot = &q->buf[q->end % ACKQ_CAPACITY];

    slot->packet_size = HEADER_SIZE;
    slot->packet.seqnum = recvbuf->acknum;
    slot->packet.rwnd = recvbuf->rwnd;

    q->end++;
    q->num_queued++;

    debug_ackq(format("Queued ack %d", slot->packet.seqnum / MAX_PAYLOAD_SIZE), q);
    return true;
}

bool ackq_pop(struct ackq *q, void (*cont)(const struct packet *, size_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    struct ack_slot *slot = &q->buf[q->begin % ACKQ_CAPACITY];
    cont(&slot->packet, slot->packet_size);

    q->begin++;
    q->num_queued--;

    debug_ackq(format("Sent ack %d", slot->packet.seqnum / MAX_PAYLOAD_SIZE), q);
    return true;
}

// Debug

void debug_recvq(const char *str, const struct recvbuf *q) {
    printf("%-32s  rwnd %6ld  begin %6ld  end %6ld  ack_index %6ld  acknum %8d\n",
           str, q->rwnd, q->begin, q->end, q->ack_index, q->acknum);
}

void debug_ackq(const char *str, const struct ackq *q) {
    printf("%-32s  queued %6ld  begin %6ld  end %6ld\n",
           str, q->num_queued, q->begin, q->end);
}
