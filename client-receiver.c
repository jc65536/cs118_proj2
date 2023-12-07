#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"

void *receive_acks(struct receiver_args *args) {
    struct sendq *sendq = args->sendq;
    struct retransq *retransq = args->retransq;
    timer_t timer = args->timer;

    // Create a UDP socket for listening
    int listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in client_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(CLIENT_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    bind(listen_sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr));

    struct packet *packet = malloc(sizeof(struct packet));

    uint32_t acknum = 0;

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1) {
            continue;
        }

        if (packet->seqnum > acknum) {
            handle_new_ACK(sendq);
            if (sendq_pop(sendq, packet->seqnum) == 0)
                unset_timer(timer); 
            else
                set_timer(timer);
            acknum = packet->seqnum;
        } else {
            if (handle_dup_ACK(sendq)) {
                retransq_push(retransq, packet->seqnum);
            }
        }
    }

    return NULL;
}
