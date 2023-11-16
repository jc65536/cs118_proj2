#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

void *receive_packets(struct receiver_args *args) {
    struct recvq *recvq = args->recvq;
    struct ackq *ackq = args->ackq;

    // Create a UDP socket for listening
    int listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        exit(1);
    }

    // Configure the server address structure
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        exit(1);
    }

    printf("Connected to proxy (recv)\n");

    struct packet *packet = malloc(sizeof(struct packet));

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1) {
            perror("Error receiving message");
            continue;
        }

        size_t payload_size = bytes_recvd - HEADER_SIZE;
        enum recv_type status = recvq_write_slot(recvq, packet, payload_size);

        switch (status) {
        case SEQ:
            ackq_push(ackq, packet->seqnum + payload_size, recvq, false);
            break;
        case RET:
            break;
        case OOO:
        case ERR:
            ackq_push(ackq, recvq_get_acknum(recvq), recvq, true);
            break;
        }

        if (is_final(packet))
            break;
    }

    printf("Received last packet\n");

    return NULL;
}
