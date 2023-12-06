#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "server.h"

/* Performance (% of time)
 * recvbuf_take_begin   60.99
 * ackq_pop             15.64
 * recvq_pop            14.08
 */

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
    atomic_size_t acknum;
    size_t bytes_read;
    struct recvbuf_slot *slot;
    bool final_read;

    struct recvbuf_slot {
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

struct recvbuf_slot *recvbuf_get_slot(const struct recvbuf *b, size_t i) {
    return b->buf + i % RECVBUF_CAPACITY;
}

void recvbuf_push(struct recvbuf *b, const struct packet *p, size_t payload_size) {
    if (p->seqnum < b->acknum || b->end + b->rwnd <= p->seqnum) {
        DBG(debug_recvbuf(format("Out of bounds %d", p->seqnum), b));
        return;
    }

    struct recvbuf_slot *slot = recvbuf_get_slot(b, p->seqnum);

    if (slot->filled) {
        DBG(debug_recvbuf(format("Duplicate %d", p->seqnum), b));
        return;
    }

    memcpy(&slot->packet, p, HEADER_SIZE + payload_size);
    slot->payload_size = payload_size;
    slot->filled = true;

    if (p->seqnum == b->acknum) {
        size_t i = b->acknum;

        while (recvbuf_get_slot(b, i)->filled)
            i++;

        b->acknum = i;
    }

    if (b->end <= p->seqnum) {
        size_t end_inc = p->seqnum + 1 - b->end;
        b->end += end_inc;
        b->rwnd -= end_inc;
    }

    DBG(debug_recvbuf(format("Received %d", p->seqnum), b));

    return;
}

bool recvbuf_pop(struct recvbuf *b, bool (*cont)(const struct packet *, size_t)) {
    if (b->begin == b->acknum) {
        return false;
    }

    struct recvbuf_slot *slot = recvbuf_get_slot(b, b->begin);
    slot->filled = false;

    if (!cont(&slot->packet, slot->payload_size))
        return false;

    b->begin++;
    b->rwnd++;

    DBG(debug_recvbuf("Wrote", b));
    return true;
}

size_t recvbuf_take_begin(struct recvbuf *b, char *dest, size_t size) {
    if (b->final_read)
        return 0;

    while (!b->slot) {
        if (b->begin < b->acknum) {
            b->slot = recvbuf_get_slot(b, b->begin);
            b->bytes_read = 0;
        }
    }

    if (b->bytes_read + size < b->slot->payload_size) {
        memcpy(dest, b->slot->packet.payload + b->bytes_read, size);
        b->bytes_read += size;
        return size;
    } else {
        size_t rem_capacity = b->slot->payload_size - b->bytes_read;
        memcpy(dest, b->slot->packet.payload + b->bytes_read, rem_capacity);
        b->final_read = is_final(&b->slot->packet);
        b->slot->filled = false;
        b->slot = NULL;
        b->begin++;
        b->rwnd++;
        return rem_capacity;
    }
}

seqnum_t recvbuf_get_acknum(const struct recvbuf *b) {
    return b->acknum;
}

size_t recvbuf_write_holes(struct recvbuf *b, char *dest, size_t size) {
    size_t bytes_written = 0;
    for (size_t i = b->acknum; i < b->end && bytes_written + sizeof(seqnum_t) <= size; i++) {
        const struct recvbuf_slot *slot = recvbuf_get_slot(b, i);
        if (!slot->filled) {
            *(seqnum_t *) (dest + bytes_written) = i;
            bytes_written += sizeof(seqnum_t);
        }
    }
    return bytes_written;
}

struct ackq {
    atomic_size_t num_queued;
    atomic_size_t begin;
    atomic_size_t end;
    seqnum_t buf[ACKQ_CAPACITY];
};

struct ackq *ackq_new() {
    return calloc(1, sizeof(struct ackq));
}

bool ackq_push(struct ackq *q, seqnum_t acknum) {
    if (q->num_queued == ACKQ_CAPACITY) {
        return false;
    }

    q->buf[q->end % ACKQ_CAPACITY] = acknum;

    q->end++;
    q->num_queued++;

    DBG(debug_ackq(format("Queued ack %d", acknum), q));
    return true;
}

bool ackq_pop(struct ackq *q, bool (*cont)(seqnum_t)) {
    if (q->num_queued == 0) {
        return false;
    }

    seqnum_t acknum = q->buf[q->begin % ACKQ_CAPACITY];

    if (!cont(acknum))
        return false;

    q->begin++;
    q->num_queued--;

    DBG(debug_ackq(format("Sent ack %d", acknum), q));
    return true;
}

// Debug

#ifdef DEBUG
void debug_recvbuf(const char *str, const struct recvbuf *b) {
    printf("[recvbuf] %-32s  rwnd %6ld  begin %6ld  end %6ld  acknum %6ld\n",
           str, b->rwnd, b->begin, b->end, b->acknum);
}
#endif

#ifdef DEBUG
void debug_ackq(const char *str, const struct ackq *q) {
    printf("[ackq] %-32s  queued %6ld  begin %6ld  end %6ld\n",
           str, q->num_queued, q->begin, q->end);
}
#endif

#ifdef DEBUG
void profile(union sigval args) {
    static int fd;
    static char str[256];
    static size_t str_size;

    if (!fd) {
        fd = creat("ignore/server-bufs.csv", S_IRUSR | S_IWUSR);
        str_size = sprintf(str, "recvq,acked,recvbuf,ackq\n");
        write(fd, str, str_size);
    }

    struct profiler_args *pargs = (struct profiler_args *) args.sival_ptr;
    const struct recvq *recvq = pargs->recvq;
    const struct recvbuf *recvbuf = pargs->recvbuf;
    const struct ackq *ackq = pargs->ackq;

    size_t rq = recvq->num_queued;
    size_t acked = recvbuf->acknum - recvbuf->begin;
    size_t rbuf = RECVBUF_CAPACITY - recvbuf->rwnd - acked;
    size_t aq = ackq->num_queued;
    str_size = sprintf(str, "%ld,%ld,%ld,%ld\n", rq, acked, rbuf, aq);
    write(fd, str, str_size);
}
#endif
