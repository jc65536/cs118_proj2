#ifndef UTILS_CLIENT_H
#define UTILS_CLIENT_H

#include "utils-common.h"

void print_send(const struct packet *pkt, bool resend) {
    if (resend)
        printf("RESEND %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
    else
        printf("SEND %d %s\n", pkt->seqnum, is_last(pkt) ? "LAST" : "");
}

#endif
