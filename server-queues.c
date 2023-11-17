#include "server.h"

struct recvq {
    atomic_size_t rwnd;
    atomic_size_t begin;
    atomic_size_t end;
    atomic_size_t ack_next;
    struct recv_slot {
        struct packet packet;
        size_t payload_size;
        bool filled;
    } *buf;
};

struct recvq *recvq_new() {
    struct recvq *q = calloc(1, sizeof(struct recvq));
    q->rwnd = RECVQ_CAPACITY;
    q->buf = calloc(RECVQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

const struct recv_slot *recvq_get_slot(struct recvq *q, size_t i) {
    return q->buf + i % RECVQ_CAPACITY;
}

enum recv_type recvq_write_slot(struct recvq *q, struct packet *p, size_t payload_size) {
    size_t packet_index = p->seqnum / MAX_PAYLOAD_SIZE;

    if (packet_index < q->ack_next || q->end + q->rwnd <= packet_index) {
        printf("Packet index %ld outside of receive window\n", packet_index);
        debug_recvq("Out", q);
        return ERR;
    }

    struct recv_slot *slot = &q->buf[packet_index % RECVQ_CAPACITY];

    if (slot->filled) {
        printf("Packet %d already received\n", p->seqnum);
        debug_recvq("Dup", q);
        return IGN;
    }

    printf("Recv packet index %ld\n", packet_index);

    memcpy(&slot->packet, p, sizeof(*p));
    slot->payload_size = payload_size;
    slot->filled = true;

    if (packet_index == q->ack_next) {
        if (q->ack_next == q->end) {
            q->ack_next++;
            q->end++;
            q->rwnd--;
        } else {
            size_t i = q->ack_next;
            while (recvq_get_slot(q, i)->filled)
                i++;
            q->ack_next = i;
        }
        debug_recvq("Seq", q);
        return SEQ;
    } else if (packet_index < q->end) {
        debug_recvq("Ret", q);
        return RET;
    } else {
        size_t rwnd_decrement = packet_index + 1 - q->end;
        q->end = packet_index + 1;
        q->rwnd -= rwnd_decrement;
        debug_recvq("Ooo", q);
        return OOO;
    }
}

bool recvq_pop(struct recvq *q, void (*cont)(const struct packet *, size_t)) {
    if (q->begin == q->ack_next) {
        return false;
    }

    struct recv_slot *slot = (struct recv_slot *) recvq_get_slot(q, q->begin);
    slot->filled = false;

    cont(&slot->packet, slot->payload_size);

    q->begin++;
    q->rwnd++;

    debug_recvq("Sent", q);
    return true;
}

struct ackq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    struct ack_slot {
        size_t packet_size;
        struct packet packet;
    } *buf;
};

struct ackq *ackq_new() {
    struct ackq *q = calloc(1, sizeof(struct ackq));
    q->buf = calloc(ACKQ_CAPACITY, sizeof(q->buf[0]));
    return q;
}

bool ackq_push(struct ackq *q, struct recvq *recvq, bool nack) {
    if (q->num_queued == ACKQ_CAPACITY) {
        debug_ackq("ackq full", q);
        return false;
    }

    struct ack_slot *slot = &q->buf[q->end];
    size_t acknum = recvq->ack_next * MAX_PAYLOAD_SIZE;

    slot->packet_size = HEADER_SIZE;
    slot->packet.seqnum = acknum;
    slot->packet.rwnd = recvq->rwnd;

    if (nack) {
        uint32_t *write_ptr = (uint32_t *) slot->packet.payload;
        size_t segnum = acknum;
        for (size_t i = recvq->ack_next; i < recvq->end; i++) {
            const struct recv_slot *recv_slot = recvq_get_slot(recvq, i);
            if (recv_slot->filled) {
                segnum += recv_slot->payload_size;
            } else {
                *write_ptr = segnum;
                segnum += MAX_PAYLOAD_SIZE;
                write_ptr++;
                slot->packet_size += sizeof(*write_ptr);
            }
        }
    }

    q->end = (q->end + 1) % ACKQ_CAPACITY;
    q->num_queued++;

    debug_ackq(nack ? "Queued NACK" : "Queued ACK", q);
    return true;
}

bool ackq_pop(struct ackq *q, void (*cont)(const struct packet *, size_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    struct ack_slot *slot = &q->buf[q->begin];

    cont(&slot->packet, slot->packet_size);

    q->begin = (q->begin + 1) % ACKQ_CAPACITY;
    q->num_queued--;

    debug_ackq("Sent", q);
    return true;
}

// Debug

void debug_recvq(char *str, const struct recvq *q) {
    printf("%s\trwnd %3ld\tbegin %3ld\tend %3ld\t\tack_next %3ld\n",
           str, q->rwnd, q->begin, q->end, q->ack_next);
}

void debug_ackq(char *str, const struct ackq *q) {
    printf("%s\tqueued %3ld\tbegin %3ld\tend %3ld\n",
           str, q->num_queued, q->begin, q->end);
}
