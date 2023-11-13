#ifndef UTILS_SERVER_H
#define UTILS_SERVER_H

#include "utils-common.h"

void print_recv(struct packet *pkt) {
    printf("RECV %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
}

#endif
