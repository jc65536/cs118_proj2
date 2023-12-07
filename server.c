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

    // Profiling
    timer_t ptimer;
    struct profiler_args profiler_args = {recvq, recvbuf, ackq};
    struct sigevent psev = {.sigev_notify = SIGEV_THREAD,
                           .sigev_value.sival_ptr = &profiler_args,
                           .sigev_notify_function = profile};
    timer_create(CLOCK_REALTIME, &psev, &ptimer);
    struct timespec tspec = {.tv_nsec = 1000000};
    struct itimerspec itspec = {.it_interval = tspec, .it_value = tspec};
    timer_settime(ptimer, 0, &itspec, NULL);

    pthread_t receiver_thread, copier_thread, writer_thread, sender_thread;

    struct receiver_args receiver_args = {recvq};
    pthread_create(&receiver_thread, NULL, (voidfn) receive_packets, &receiver_args);

    struct copier_args copier_args = {recvq, recvbuf, ackq};
    pthread_create(&copier_thread, NULL, (voidfn) copy_packets, &copier_args);

    struct writer_args writer_args = {recvbuf};
    pthread_create(&writer_thread, NULL, (voidfn) decompress_and_write, &writer_args);

    struct sender_args sender_args = {ackq, recvbuf};
    pthread_create(&sender_thread, NULL, (voidfn) send_acks, &sender_args);

    pthread_join(copier_thread, NULL);
    pthread_join(writer_thread, NULL);
    // pthread_join(sender_thread, NULL);
}
