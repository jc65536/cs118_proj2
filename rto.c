#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "rto.h"

#define A 0.125
#define B 0.25

#define RTO_LB 1000000000

volatile struct timespec rto = (struct timespec){.tv_sec = RTO_LB / S_TO_NS, .tv_nsec = RTO_LB % S_TO_NS};

static int consecutive_doubling = 0;
volatile bool lossy_link = false;

static bool flag = false;
static struct timespec tspec;

static seqnum_t stored_seqnum;

uint64_t abs_diff(uint64_t x, uint64_t y) {
    return x < y ? y - x : x - y;
}

uint64_t to_uint(struct timespec t) {
    return t.tv_sec * S_TO_NS + t.tv_nsec;
}

struct timespec to_tspec(uint64_t t) {
    return (struct timespec) {.tv_sec = t / S_TO_NS, .tv_nsec = t % S_TO_NS};
}

void log_send(seqnum_t seqnum) {
    if (lossy_link || flag)
        return;

    stored_seqnum = seqnum;
    clock_gettime(CLOCK_REALTIME, &tspec);

    flag = true;
}

static uint64_t srtt;
static uint64_t rttvar;

void log_ack(seqnum_t acknum) {
    if (lossy_link || !flag || acknum <= stored_seqnum)
        return;

    struct timespec endspec = {};
    clock_gettime(CLOCK_REALTIME, &endspec);
    uint64_t rtt = to_uint(endspec) - to_uint(tspec);

    if (srtt == 0) {
        srtt = rtt;
        rttvar = rtt / 2;
    } else {
        rttvar = (1 - B) * rttvar + B * abs_diff(srtt, rtt);
        srtt = (1 - A) * srtt + A * rtt;
    }

    uint64_t rto_ = srtt + 2 * rttvar;

    if (rto_ < RTO_LB)
        rto_ = RTO_LB;

    rto = to_tspec(rto_);
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

    if (consecutive_doubling == 1) {
#ifdef DEBUG
        printf("Lossy link detected!!\n");
#endif
        rto = (struct timespec){.tv_sec = 0, .tv_nsec = S_TO_NS / 10};
        lossy_link = true;
        return;
    }

    // rto = to_tspec(to_uint(rto) * 2);
    flag = false;
    consecutive_doubling++;
}
