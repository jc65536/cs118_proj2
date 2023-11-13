#ifndef SERVER_H
#define SERVER_H

#include "common.h"

static inline void print_recv(struct packet *pkt) {
    printf("RECV %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
}

#endif
