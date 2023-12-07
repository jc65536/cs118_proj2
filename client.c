#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client.h"

int main(int argc, char *argv[]) {
    char *filename = argv[1];

    // TODO: Read from file, and initiate reliable data transfer to the server
    struct sendq *sendq = sendq_new();
    struct retransq *retransq = retransq_new();

    timer_t timer;

    struct timer_args timer_args = {sendq, retransq};

    struct sigevent sev = {.sigev_notify = SIGEV_THREAD,
                           .sigev_value.sival_ptr = &timer_args,
                           .sigev_notify_function = handle_timer};

    timer_create(CLOCK_REALTIME, &sev, &timer);

    pthread_t reader_thread, sender_thread, receiver_thread;

    struct reader_args reader_args = {sendq, filename};
    pthread_create(&reader_thread, NULL, (voidfn) read_and_compress, &reader_args);

    struct sender_args sender_args = {sendq, retransq, timer};
    pthread_create(&sender_thread, NULL, (voidfn) send_packets, &sender_args);

    struct receiver_args receiver_args = {sendq, retransq, timer}; //TODO: add state arg for slow start, congestion control, and fast recovery
    pthread_create(&receiver_thread, NULL, (voidfn) receive_acks, &receiver_args);

    pthread_join(reader_thread, NULL);
    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);
}
