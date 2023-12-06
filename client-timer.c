#include <time.h>

#include "client.h"
#include "rto.h"

bool timer_set = false;

void handle_timer(union sigval args) {
    struct timer_args *targs = (struct timer_args *) args.sival_ptr;
    struct sendq *sendq = targs->sendq;
    struct retransq *retransq = targs->retransq;
    timer_t timer = targs->timer;

#ifdef DEBUG
    printf("Timeout!!\n");
#endif

    if (holes_len) {
        for (size_t i = 0; i < holes_len; i++)
            retransq_push(retransq, holes[i]);

        holes_len = 0;
    } else {
        const struct packet *p = sendq_oldest_packet(sendq);

        if (p)
            retransq_push(retransq, p->seqnum);
    }

    if (!lossy_link) {
        sendq_halve_ssthresh(sendq);
        sendq_set_cwnd(sendq, 1);
        double_rto();
    }

    set_timer(timer);
}

void set_timer(timer_t t) {
    // 10000000 ns = 10 ms
    struct itimerspec itspec = {.it_interval = rto, .it_value = rto};
    timer_settime(t, 0, &itspec, NULL);
    timer_set = true;
}

void unset_timer(timer_t t) {
    struct itimerspec itspec = {};
    timer_settime(t, 0, &itspec, NULL);
    timer_set = false;
}
