#include <time.h>

#include "client.h"

void handle_timer(union sigval args) {
    struct timer_args *targs = (struct timer_args *) args.sival_ptr;
    struct sendq *sendq = targs->sendq;
    struct retransq *retransq = targs->retransq;

#ifdef DEBUG
    printf("Timeout!!\n");
#endif
    
    // p is the oldest in-flight packet, or NULL if there are no in-flight
    // packets.
    const struct packet *p = sendq_oldest_packet(sendq);

    if (p)
        retransq_push(retransq, p->seqnum);
}

void set_timer(timer_t t) {
    // 10000000 ns = 10 ms
    struct timespec tspec = {.tv_nsec = TIMEOUT};
    struct itimerspec itspec = {.it_interval = tspec, .it_value = tspec};
    timer_settime(t, 0, &itspec, NULL);
}

void unset_timer(timer_t t) {
    struct itimerspec itspec = {};
    timer_settime(t, 0, &itspec, NULL);
}

bool is_timer_set(timer_t t) {
    struct itimerspec itspec;
    timer_gettime(t, &itspec);
    return !(itspec.it_value.tv_sec + itspec.it_value.tv_nsec == 0);
}
