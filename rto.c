#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "rto.h"

#define S_TO_NS ((uint64_t) 1000000000)
#define RTO_LB ((uint64_t) 500000000)
#define A 0.125
#define B 0.25

struct timespec rto = (struct timespec){.tv_sec = 0, .tv_nsec = RTO_LB};

static int consecutive_doubling = 0;
bool lossy_link = false;

static bool flag = false;
static struct timespec tspec;

static seqnum_t stored_seqnum;

uint64_t abs_diff(uint64_t x, uint64_t y) {
    return x < y ? y - x : x - y;
}

void log_send(seqnum_t seqnum) {
    if (lossy_link || flag)
        return;

    stored_seqnum = seqnum;
    clock_gettime(CLOCK_REALTIME, &tspec);

    flag = true;
}

void log_ack(seqnum_t acknum) {
    static uint64_t srtt;
    static uint64_t rttvar;

    if (lossy_link || !flag || acknum <= stored_seqnum)
        return;

    struct timespec endspec = {};
    clock_gettime(CLOCK_REALTIME, &endspec);
    uint64_t rtt = (endspec.tv_sec - tspec.tv_sec) * S_TO_NS + (endspec.tv_nsec - tspec.tv_nsec);

    if (srtt == 0) {
        srtt = rtt;
        rttvar = rtt / 2;
    } else {
        rttvar = (1 - B) * rttvar + B * abs_diff(srtt, rtt);
        srtt = (1 - A) * srtt + A * rtt;
    }

    uint64_t rto_ = srtt + 2 * rttvar;

    if (rto_ < RTO_LB) {
        rto.tv_sec = 0;
        rto.tv_nsec = RTO_LB;
    } else {
        rto.tv_sec = rto_ / S_TO_NS;
        rto.tv_nsec = rto_ % S_TO_NS;
    }

    consecutive_doubling = 0;

#ifdef DEBUG
    printf("[RTO] Updated to %ld s %ld ns; srtt = %ld, rttvar = %ld, rtt = %ld\n",
           rto.tv_sec, rto.tv_nsec, srtt, rttvar, rtt);
#endif

    flag = false;
}

void double_rto() {
    if (lossy_link)
        return;

    if (consecutive_doubling == 2) {
#ifdef DEBUG
        printf("Lossy link detected!!\n");
#endif
        rto = (struct timespec){.tv_sec = 0, .tv_nsec = S_TO_NS / 10};
        lossy_link = true;
        return;
    }

    flag = false;
    consecutive_doubling++;
}
