#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

static int listen_sockfd;

void receive_one(struct packet *p, size_t *packet_size) {
    ssize_t bytes_recvd = recv(listen_sockfd, p, sizeof(struct packet), 0);

    if (bytes_recvd == -1) {
        perror("Error receiving message");
        *packet_size = 0;
    } else {
        *packet_size = bytes_recvd;
    }
}

void *receive_packets(struct receiver_args *args) {
    struct recvq *recvq = args->recvq;

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    while (true)
        recvq_write(recvq, receive_one);
}
