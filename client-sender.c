#include <arpa/inet.h>

#include "client.h"

void *send_packets(struct sender_args *args) {
    struct sendq *sendq = args->sendq;
    struct retransq *retransq = args->retransq;

    int send_sockfd;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        exit(1);
    }

    // Configure the server address structure to which we will send data
    struct sockaddr_in server_addr_to = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT_TO),
        .sin_addr.s_addr = inet_addr(SERVER_IP)};

    if (connect(send_sockfd, (struct sockaddr *) &server_addr_to, sizeof(server_addr_to)) == -1) {
        perror("Failed to connect to proxy");
        close(send_sockfd);
        exit(1);
    }

    while (true) {
        if (sendq->send_next == sendq->end)
            continue;

        struct packet *packet = &sendq->buf[sendq->send_next];
        ssize_t bytes_sent = send(send_sockfd, packet, HEADER_SIZE + packet->payload_size, 0);

        if (bytes_sent == -1)
            perror("Error sending message");
        else
            print_send(packet, false);
        
        sendq->send_next = (sendq->send_next + 1) % SENDQ_CAPACITY;

        if (is_last(packet))
            break;
    }

    return NULL;
}
