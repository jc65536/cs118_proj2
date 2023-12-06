#ifndef RTO_H
#define RTO_H

#include <stdint.h>
#include <time.h>

#include "common.h"

extern struct timespec rto;
extern bool lossy_link;

void log_send(seqnum_t seqnum);
void log_ack(seqnum_t acknum);
void double_rto();

#endif
