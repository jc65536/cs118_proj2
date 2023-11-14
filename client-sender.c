#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"

void *send_packets(struct sender_args *args) {
    struct sendq *sendq = args->sendq;
    struct retransq *retransq = args->retransq;

    // Create a UDP socket for sending
    int send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    printf("Connected to proxy (send)\n");

    while (true) {
        if (retransq->num_queued > 0) {
            size_t seqnum = retransq->buf[retransq->begin];

            size_t packet_index = sendq->begin + (seqnum - sendq->buf[sendq->begin].packet.seqnum) / MAX_PAYLOAD_SIZE;

            size_t packet_size = sendq->buf[packet_index].packet_size;
            struct packet *packet = &sendq->buf[packet_index].packet;

            ssize_t bytes_sent = send(send_sockfd, packet, packet_size, 0);

            if (bytes_sent == -1)
                perror("Error sending message");
            else
                print_send(packet, true);

            retransq->begin = (retransq->begin + 1) % RETRANSQ_CAPACITY;
            retransq->num_queued--;
        } else if (sendq->send_next != sendq->end) {
            size_t packet_size = sendq->buf[sendq->send_next].packet_size;
            struct packet *packet = &sendq->buf[sendq->send_next].packet;
            ssize_t bytes_sent = send(send_sockfd, packet, packet_size, 0);

            if (bytes_sent == -1)
                perror("Error sending message");
            else
                print_send(packet, false);

            sendq->send_next = (sendq->send_next + 1) % SENDQ_CAPACITY;

            if (is_final(packet))
                break;
        }
    }

    printf("Sent last packet\n");

    return NULL;
}
