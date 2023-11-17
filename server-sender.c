#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

static int send_sockfd;

void send_one(const struct packet *p, size_t packet_size) {
    ssize_t bytes_sent = send(send_sockfd, p, packet_size, 0);

    if (bytes_sent == -1)
        printf("Error sending ACK\n");
}

void *send_acks(struct sender_args *args) {
    struct ackq *ackq = args->ackq;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    while (true)
        ackq_pop(ackq, send_one);

    return NULL;
}
