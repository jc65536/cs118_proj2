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

const struct recv_slot *recvbuf_get_slot(const struct recvbuf *b, size_t i) {
    return b->buf + i % RECVBUF_CAPACITY;
}

enum recv_type recvbuf_write(struct recvbuf *b, const struct packet *p, size_t payload_size) {
    size_t packet_index = p->seqnum / MAX_PAYLOAD_SIZE;

    if (packet_index < b->ack_index || b->end + b->rwnd <= packet_index) {
        printf("Packet index %ld outside of receive window\n", packet_index);
        debug_recvq("Out", b);
        return ERR;
    }

    struct recv_slot *slot = &b->buf[packet_index % RECVBUF_CAPACITY];

    if (slot->filled) {
        printf("Packet %ld already received\n", packet_index);
        debug_recvq("Dup", b);
        return ERR;
    }

    printf("Recv packet index %ld\n", packet_index);

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

    debug_recvq("Received", b);

    return ret;
}

bool recvbuf_pop(struct recvbuf *b, void (*cont)(const struct packet *, size_t)) {
    if (b->begin == b->ack_index) {
        return false;
    }

    struct recv_slot *slot = (struct recv_slot *) recvbuf_get_slot(b, b->begin);
    slot->filled = false;

    cont(&slot->packet, slot->payload_size);

    b->begin++;
    b->rwnd++;

    debug_recvq("Sent", b);
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
        debug_ackq("ackq full", q);
        return false;
    }

    struct ack_slot *slot = &q->buf[q->end % ACKQ_CAPACITY];

    slot->packet_size = HEADER_SIZE;
    slot->packet.seqnum = recvbuf->acknum;
    slot->packet.rwnd = recvbuf->rwnd;

    q->end++;
    q->num_queued++;

    debug_ackq("Queued ACK", q);
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

    debug_ackq("Sent", q);
    return true;
}

// Debug

void debug_recvq(char *str, const struct recvbuf *q) {
    printf("%s\trwnd %3ld\tbegin %3ld\tend %3ld\t\tack_index %3ld\tacknum %5d\n",
           str, q->rwnd, q->begin, q->end, q->ack_index, q->acknum);
}

void debug_ackq(char *str, const struct ackq *q) {
    printf("%s\tqueued %3ld\tbegin %3ld\tend %3ld\n",
           str, q->num_queued, q->begin, q->end);
}
