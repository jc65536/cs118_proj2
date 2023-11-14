#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

void *send_acks(struct sender_args *args) {
    struct ackq *ackq = args->ackq;

    // Create a UDP socket for sending
    int send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        exit(1);
    }

    // Configure the client address structure to which we will send ACKs
    struct sockaddr_in client_addr_to = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(LOCAL_HOST),
        .sin_port = htons(CLIENT_PORT_TO)};

    if (connect(send_sockfd, (struct sockaddr *) &client_addr_to, sizeof(client_addr_to)) == -1) {
        perror("Failed to connect to proxy");
        close(send_sockfd);
        exit(1);
    }

    while (true) {
        if (ackq->num_queued == 0)
            continue;

        size_t packet_size = ackq->buf[ackq->begin].packet_size;
        struct packet *packet = &ackq->buf[ackq->begin].packet;

        ssize_t bytes_sent = send(send_sockfd, packet, packet_size, 0);

        if (bytes_sent == -1)
            perror("Error sending message");
        else
            printf("Sent ACK\n");

        ackq->begin = (ackq->begin + 1) % ACKQ_CAPACITY;
        ackq->num_queued--;
    }
}
