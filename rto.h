#ifndef RTO_H
#define RTO_H

#include <stdint.h>
#include <time.h>

#include "common.h"

#define S_TO_NS ((uint64_t) 1000000000)

extern volatile struct timespec rto;

void log_send(seqnum_t seqnum);
void log_ack(seqnum_t acknum);
void double_rto();

#endif
