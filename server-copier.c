#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

static struct recvbuf *recvbuf;
static struct ackq *ackq;

void copy_one(const struct packet *p, size_t packet_size) {
    size_t payload_size = packet_size - HEADER_SIZE;
    seqnum_t acknum = recvbuf_push(recvbuf, p, payload_size);

    if (!(p->flags & FLAG_NOACK))
        ackq_push(ackq, acknum);
}

void *copy_packets(struct copier_args *args) {
    struct recvq *recvq = args->recvq;
    recvbuf = args->recvbuf;
    ackq = args->ackq;

    while (true)
        recvq_pop(recvq, copy_one);
}
