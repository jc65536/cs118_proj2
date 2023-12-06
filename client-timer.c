#include <time.h>

#include "client.h"
#include "rto.h"

void handle_timer(union sigval args) {
    // static const struct packet *last_retrans_packet;

    struct timer_args *targs = (struct timer_args *) args.sival_ptr;
    struct sendq *sendq = targs->sendq;
    struct retransq *retransq = targs->retransq;
    timer_t timer = targs->timer;

#ifdef DEBUG
    printf("Timeout!!\n");
#endif

    // p is the oldest in-flight packet, or NULL if there are no in-flight
    // packets.
    // const struct packet *p = sendq_oldest_packet(sendq);

    if (holes_len) {
        // retransq_push(retransq, p->seqnum);

        for (size_t i = 0; i < holes_len; i++) {
            retransq_push(retransq, holes[i]);
        }

        holes_len = 0;

        // last_retrans_packet = p;
    }

    // if (p != last_retrans_packet)
    sendq_halve_ssthresh(sendq);

    sendq_set_cwnd(sendq, 1);
    double_rto();
    set_timer(timer);
}

void set_timer(timer_t t) {
    // 10000000 ns = 10 ms
    struct itimerspec itspec = {.it_interval = rto, .it_value = rto};
    timer_settime(t, 0, &itspec, NULL);
}

void unset_timer(timer_t t) {
    struct itimerspec itspec = {};
    timer_settime(t, 0, &itspec, NULL);
}

bool is_timer_set(timer_t t) {
    struct itimerspec itspec;
    timer_gettime(t, &itspec);
    return itspec.it_value.tv_sec | itspec.it_value.tv_nsec;
}
