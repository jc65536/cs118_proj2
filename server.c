#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server.h"

int main() {
    // TODO: Receive file from the client and save it as output.txt

    struct recvq *recvq = recvq_new();
    struct recvbuf *recvbuf = recvbuf_new();
    struct ackq *ackq = ackq_new();

#ifdef DEBUG
    // Profiling
    timer_t ptimer;
    struct profiler_args profiler_args = {recvq, recvbuf, ackq};
    struct sigevent psev = {.sigev_notify = SIGEV_THREAD,
                            .sigev_value.sival_ptr = &profiler_args,
                            .sigev_notify_function = profile};
    timer_create(CLOCK_REALTIME, &psev, &ptimer);
    struct timespec tspec = {.tv_nsec = 50000000};
    struct itimerspec itspec = {.it_interval = tspec, .it_value = tspec};
    timer_settime(ptimer, 0, &itspec, NULL);
#endif

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // pthread_attr_setschedpolicy(&attr, SCHED_RR);
    // struct sched_param param = {1};
    // pthread_attr_setschedparam(&attr, &param);

    pthread_t receiver_thread, copier_thread, writer_thread, sender_thread;

    struct receiver_args receiver_args = {recvq};
    pthread_create(&receiver_thread, &attr, (voidfn) receive_packets, &receiver_args);

    struct copier_args copier_args = {recvq, recvbuf, ackq};
    pthread_create(&copier_thread, &attr, (voidfn) copy_packets, &copier_args);

    struct writer_args writer_args = {recvbuf};
    pthread_create(&writer_thread, &attr, (voidfn) write_file, &writer_args);

    struct sender_args sender_args = {ackq, recvbuf};
    pthread_create(&sender_thread, &attr, (voidfn) send_acks, &sender_args);

    pthread_join(writer_thread, NULL);
}
