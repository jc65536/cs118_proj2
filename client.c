#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "utils-client.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }

    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    struct sockaddr_in server_addr_to = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT_TO),
        .sin_addr.s_addr = inet_addr(SERVER_IP)};

    // Configure the client address structure
    struct sockaddr_in client_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(CLIENT_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    if (connect(send_sockfd, (struct sockaddr *) &server_addr_to, sizeof(server_addr_to)) == -1) {
        perror("Failed to connect to proxy");
        close(send_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server

    struct packet *packet = calloc(1, sizeof(struct packet) + MAX_PAYLOAD_SIZE);

    do {
        size_t bytes_read = fread(packet->payload, 1, MAX_PAYLOAD_SIZE, fp);

        if (bytes_read != MAX_PAYLOAD_SIZE && feof(fp))
            packet->flags = FLAG_FINAL;

        ssize_t bytes_sent = send(send_sockfd, packet, sizeof(struct packet) + bytes_read, 0);

        if (bytes_sent == -1)
            perror("Error sending message");
        else
            print_send(packet, false);

        packet->seqnum += bytes_read;

        // Pause for 5 ms so server isn't overloaded
        usleep(5000);
    } while (!is_last(packet));

    free(packet);
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
