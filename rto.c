#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "rto.h"

#define S_TO_NS 1000000000
#define A 0.125
#define B 0.25

struct timespec rto = (struct timespec){.tv_sec = 1, .tv_nsec = 0};

static bool flag = false;
static struct timespec tspec;

static uint32_t stored_seqnum;

uint64_t abs_diff(uint64_t x, uint64_t y) {
    return x < y ? y - x : x - y;
}

void log_send(uint32_t seqnum) {
    if (flag)
        return;

    stored_seqnum = seqnum;
    clock_gettime(CLOCK_REALTIME, &tspec);

    flag = true;
}

void log_ack(uint32_t acknum) {
    static uint64_t srtt;
    static uint64_t rttvar;

    if (!flag || acknum <= stored_seqnum)
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

    uint64_t rto_ = srtt + 4 * rttvar;

    if (rto_ < S_TO_NS) {
        rto.tv_sec = 1;
        rto.tv_nsec = 0;
    } else {
        rto.tv_sec = rto_ / S_TO_NS;
        rto.tv_nsec = rto_ % S_TO_NS;
    }

#ifdef DEBUG
    printf("[RTO] Updated to %ld s %ld ns; srtt = %ld, rttvar = %ld, rtt = %ld\n",
           rto.tv_sec, rto.tv_nsec, srtt, rttvar, rtt);
#endif

    flag = false;
}

void double_rto() {
    uint64_t rto_ = rto.tv_sec * S_TO_NS + rto.tv_nsec;
    rto_ *= 2;
    rto.tv_sec = rto_ / S_TO_NS;
    rto.tv_nsec = rto_ % S_TO_NS;
    flag = false;
}
