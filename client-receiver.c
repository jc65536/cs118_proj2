#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"

void *receive_acks(struct receiver_args *args) {
    struct sendq *sendq = args->sendq;
    struct retransq *retransq = args->retransq;
    //TODO: get state from args
    timer_t timer = args->timer;

    // Create a UDP socket for listening
    int listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        exit(1);
    }

    // Configure the client address structure
    struct sockaddr_in client_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(CLIENT_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1) {
        perror("Bind failed");
        close(listen_sockfd);
        exit(1);
    }

    printf("Connected to proxy (recv)\n");

    struct packet *packet = malloc(sizeof(struct packet));

    uint32_t acknum = 0;
    int dupcount = 0;

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1) {
            perror("Error receiving message");
            continue;
        }

        if (packet->seqnum > acknum) { //new ack
        //TODO:
            //if in slow start, inc cwnd by 1
            //else if in congestion control, inc cwnd by 1/cwnd
            //else if in fast recovery, cwnd = ssthresh
            //for all, set dupcount = 0
            dupcount = 0;
            if (sendq_pop(sendq, packet->seqnum) == 0)
                unset_timer(timer);
            acknum = packet->seqnum;
        } else {
            //TODO: if in fast recovery, dup ack --> inc cwnd
            //else: inc dup count, check if dup == 3 and go to FR
            dupcount++;
            if (dupcount == 3) { 
                printf("3 duplicate acks!\n");
                retransq_push(retransq, packet->seqnum);
            }
        }
    }

    return NULL;
}
