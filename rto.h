#ifndef RTO_H
#define RTO_H

#include <stdint.h>
#include <time.h>

extern struct timespec rto;
extern bool is_lossy_link;

void log_send(uint32_t seqnum);
void log_ack(uint32_t acknum);
void double_rto();

#endif
