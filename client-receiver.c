#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"

void *receive_acks(struct receiver_args *args) {
    struct sendq *sendq = args->sendq;
    struct retransq *retransq = args->retransq;

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

    while (true) {
        if (sendq->num_queued == 0)
            continue;

        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1)
            perror("Error receiving message");
        else
            print_recv(packet);

        size_t seq_diff = packet->seqnum - sendq->buf[sendq->begin].packet.seqnum;
        size_t forward_amt = (seq_diff - 1) / MAX_PAYLOAD_SIZE + 1; // Divide and round up
        sendq->begin += forward_amt;

        size_t payload_size = bytes_recvd - HEADER_SIZE;

        if (payload_size == 0) {
            // Positive ACK
        } else {
            // Negative ACK
            size_t seqnum_count = payload_size / sizeof(uint32_t);
        }
    }
    return NULL;
}
