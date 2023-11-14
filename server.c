#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "server.h"

struct recvq make_recvq() {
    struct recvq q = {};
    q.rwnd = RECVQ_CAPACITY;
    q.buf = calloc(RECVQ_CAPACITY, sizeof(q.buf[0]));
    return q;
}

struct ackq make_ackq() {
    struct ackq q = {};
    q.buf = calloc(ACKQ_CAPACITY, sizeof(q.buf[0]));
    return q;
}

int main() {
    int listen_sockfd, send_sockfd;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    struct sockaddr_in client_addr_to = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(LOCAL_HOST),
        .sin_port = htons(CLIENT_PORT_TO)};

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt

    struct recvq recvq = make_recvq();
    struct ackq ackq = make_ackq();

    pthread_t receiver_thread;

    struct receiver_args receiver_args = {.recvq = &recvq, .ackq = &ackq};
    pthread_create(&receiver_thread, NULL, (voidfn) receive_packets, &receiver_args);

    pthread_join(receiver_thread, NULL);

    struct packet *packet = calloc(1, sizeof(struct packet));

    do {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1)
            perror("Error receiving message");
        else
            print_recv(packet);

        size_t payload_size = bytes_recvd - HEADER_SIZE;
        size_t bytes_written = fwrite(packet->payload, sizeof(char), payload_size, fp);

        if (bytes_written != payload_size)
            perror("Error writing output");
    } while (!is_last(packet));

    free(packet);
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
