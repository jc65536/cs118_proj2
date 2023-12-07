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

    uint32_t acknum = 0;

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1) {
            perror("Error receiving message");
            continue;
        }

        if (packet->seqnum > acknum) { //new ack
            handle_new_ACK(sendq); //updates cwnd, dupACKs, 
            if (sendq_pop(sendq, packet->seqnum) == 0) //no more packets in queue
                unset_timer(timer); 
            else
                set_timer(timer); //restart timer 
            acknum = packet->seqnum;
        } else {
            ;
            //udpates dupACKs, cwnd, and checks if dupACKs == 3
            if (handle_dup_ACK(sendq)) {
            #ifdef DEBUG
                printf("3 duplicate acks!\n");
            #endif

                // Push the acknum of the duplicate acks onto retransmission
                // queue. sender_thread will take care of the actual
                // retransmission.
                retransq_push(retransq, packet->seqnum);
            }
        }
    }

    return NULL;
}
