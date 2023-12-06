#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef DEBUG
void term(int) {
    exit(0);
}
#endif

#include "client.h"

int main(int argc, char *argv[]) {
    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }

#ifdef DEBUG
    signal(SIGTERM, term);
#endif

    char *filename = argv[1];

    // TODO: Read from file, and initiate reliable data transfer to the server
    struct sendq *sendq = sendq_new();
    struct retransq *retransq = retransq_new();

    timer_t timer;

    struct timer_args timer_args = {sendq, retransq, timer};

    struct sigevent sev = {.sigev_notify = SIGEV_THREAD,
                           .sigev_value.sival_ptr = &timer_args,
                           .sigev_notify_function = handle_timer};

    timer_create(CLOCK_REALTIME, &sev, &timer);

#ifdef DEBUG
    // Profiling
    timer_t ptimer;
    struct profiler_args profiler_args = {sendq, retransq};
    struct sigevent psev = {.sigev_notify = SIGEV_THREAD,
                            .sigev_value.sival_ptr = &profiler_args,
                            .sigev_notify_function = profile};
    timer_create(CLOCK_REALTIME, &psev, &ptimer);
    struct timespec tspec = {.tv_nsec = 60000000};
    struct itimerspec itspec = {.it_interval = tspec, .it_value = tspec};
    timer_settime(ptimer, 0, &itspec, NULL);
#endif

    pthread_t reader_thread, sender_thread, receiver_thread;

    struct reader_args reader_args = {sendq, filename};
    pthread_create(&reader_thread, NULL, (voidfn) read_and_compress, &reader_args);

    struct sender_args sender_args = {sendq, retransq, timer};
    pthread_create(&sender_thread, NULL, (voidfn) send_packets, &sender_args);

    struct receiver_args receiver_args = {sendq, retransq, timer};
    pthread_create(&receiver_thread, NULL, (voidfn) receive_acks, &receiver_args);

    pthread_join(reader_thread, NULL);
    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);
}
