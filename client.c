#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "client.h"

struct sendq make_sendq() {
    struct sendq q = {};
    q.buf = calloc(SENDQ_CAPACITY, sizeof(q.buf[0]));
    return q;
}

int main(int argc, char *argv[]) {
    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }

    char *filename = argv[1];

    // TODO: Read from file, and initiate reliable data transfer to the server
    struct sendq sendq = make_sendq();
    struct retransq retransq = {};

    pthread_t reader_thread, sender_thread, receiver_thread;

    struct reader_args reader_args = {.sendq = &sendq, .filename = filename};
    pthread_create(&reader_thread, NULL, (voidfn) read_file, &reader_args);

    struct sender_args sender_args = {.sendq = &sendq, .retransq = &retransq};
    pthread_create(&sender_thread, NULL, (voidfn) send_packets, &sender_args);

    struct receiver_args receiver_args = {.sendq = &sendq, .retransq = &retransq};
    pthread_create(&receiver_thread, NULL, (voidfn) receive_acks, &receiver_args);

    pthread_join(reader_thread, NULL);
    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);
}
