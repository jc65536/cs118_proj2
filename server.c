#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils-server.h"

int main() {
    int listen_sockfd, send_sockfd;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    // Configure the client address structure to which we will send ACKs
    struct sockaddr_in client_addr_to = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(LOCAL_HOST),
        .sin_port = htons(CLIENT_PORT_TO)};

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt

    struct packet *packet = calloc(1, sizeof(struct packet) + MAX_PAYLOAD_SIZE);

    do {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet) + MAX_PAYLOAD_SIZE, 0);

        if (bytes_recvd == -1)
            perror("Error receiving message");
        else
            print_recv(packet);

        size_t payload_size = bytes_recvd - sizeof(struct packet);
        size_t bytes_written = fwrite(packet->payload, 1, payload_size, fp);

        if (bytes_written != payload_size)
            perror("Error writing output");
    } while (!is_last(packet));

    free(packet);
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
