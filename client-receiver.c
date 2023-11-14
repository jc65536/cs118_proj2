#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"

void queue_rt(struct retransq *q, size_t seqnum) {
    if (q->num_queued == RETRANSQ_CAPACITY) {
        printf("Retransmit queue full\n");
        return;
    }

    q->buf[q->end] = seqnum;
    q->end = (q->end + 1) % RETRANSQ_CAPACITY;
    q->num_queued++;
}

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
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1)
            perror("Error receiving message");

        size_t seq_diff = packet->seqnum - sendq->buf[sendq->begin].packet.seqnum;
        size_t forward_amt = (seq_diff - 1) / MAX_PAYLOAD_SIZE + 1; // Divide and round up
        sendq->begin += forward_amt;
        sendq->num_queued -= forward_amt;

        size_t payload_size = bytes_recvd - HEADER_SIZE;

        if (payload_size == 0) {
            printf("ACK\tseq %7d\trwnd %3d\tqueued %3ld\n", packet->seqnum, packet->rwnd, sendq->num_queued);
        } else {
            // Negative ACK
            size_t seqnum_count = payload_size / sizeof(uint32_t);
            uint32_t *lost_seqnums = (uint32_t *) packet->payload;

            for (size_t i = 0; i < seqnum_count; i++)
                queue_rt(retransq, lost_seqnums[i]);
            
            printf("NACK\tseq %7d\tdropped %3ld\n", packet->seqnum, seqnum_count);
        }
    }

    return NULL;
}
