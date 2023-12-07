#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

static struct recvbuf *recvbuf;
static struct ackq *ackq;

void copy_one(const struct packet *p, size_t packet_size) {
    size_t payload_size = packet_size - HEADER_SIZE;
    recvbuf_push(recvbuf, p, payload_size);
    ackq_push(ackq, recvbuf_get_acknum(recvbuf));
}

void *copy_packets(struct copier_args *args) {
    struct recvq *recvq = args->recvq;
    recvbuf = args->recvbuf;
    ackq = args->ackq;

    while (true)
        recvq_pop(recvq, copy_one);

    return NULL;
}
