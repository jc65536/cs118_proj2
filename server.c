#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "server.h"

int main() {
    // TODO: Receive file from the client and save it as output.txt

    struct recvq *recvq = recvq_new();
    struct ackq *ackq = ackq_new();

    pthread_t receiver_thread, writer_thread, sender_thread;

    struct receiver_args receiver_args = {.recvq = recvq, .ackq = ackq};
    pthread_create(&receiver_thread, NULL, (voidfn) receive_packets, &receiver_args);

    struct writer_args writer_args = {.recvq = recvq};
    pthread_create(&writer_thread, NULL, (voidfn) write_file, &writer_args);

    struct sender_args sender_args = {.ackq = ackq};
    pthread_create(&sender_thread, NULL, (voidfn) send_acks, &sender_args);

    pthread_join(receiver_thread, NULL);
    pthread_join(writer_thread, NULL);
    // pthread_join(sender_thread, NULL);
}
