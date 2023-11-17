#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

static struct recvbuf *recvbuf;
static struct ackq *ackq;

void copy_one(const struct packet *p, size_t packet_size) {
    size_t payload_size = packet_size - HEADER_SIZE;
    enum recv_type status = recvbuf_write(recvbuf, p, payload_size);

    switch (status) {
    case SEQ:
        ackq_push(ackq, recvbuf, false);
        break;
    case RET:
        break;
    case OOO:
    case ERR:
        ackq_push(ackq, recvbuf, true);
        break;
    }
}

void *copy_packets(struct copier_args *args) {
    struct recvq *recvq = args->recvq;
    recvbuf = args->recvbuf;
    ackq = args->ackq;

    while (true)
        recvq_pop(recvq, copy_one);

    return NULL;
}
