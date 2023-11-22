#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"

void *receive_acks(struct receiver_args *args) {
    struct sendq *sendq = args->sendq;
    struct retransq *retransq = args->retransq;
    timer_t timer = args->timer;

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

    // acknum is always set to the largest acknum we have received from the
    // server. Represents how many bytes we know the server has received.
    uint32_t acknum = 0;

    int dupcount = 0;

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1) {
            perror("Error receiving message");
            continue;
        }

        if (packet->seqnum > acknum) {
            // We received an ack for new data, so we can pop the packets in our
            // buffer until the latest acknum

            dupcount = 0;

            // sendq_pop returns the number of in-flight packets (packets sent
            // but not yet acked). If this number is 0, we can disarm the timer.
            if (sendq_pop(sendq, packet->seqnum) == 0)
                unset_timer(timer);

            // Update acknum to the latest acknum
            acknum = packet->seqnum;
        } else {
            dupcount++;
            if (dupcount == 3) {
                printf("3 duplicate acks!\n");

                // Push the acknum of the duplicate acks onto retransmission
                // queue. sender_thread will take care of the actual
                // retransmission.
                retransq_push(retransq, packet->seqnum);
            }
        }
    }

    return NULL;
}
