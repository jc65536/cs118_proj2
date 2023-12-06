#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"
#include "rto.h"

enum trans_state {
    SLOW_START,
    CONGESTION_AVOIDANCE,
    FAST_RECOVERY
};

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
    uint32_t last_acknum = 0;
    uint32_t acknum = 0;
    enum trans_state state = SLOW_START;

    int dupcount = 0;

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1) {
            perror("Error receiving message");
            continue;
        }

        log_ack(packet->seqnum);

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

            switch (state) {
            case SLOW_START:
                if (sendq_inc_cwnd(sendq) > sendq_get_ssthresh(sendq))
                    state = CONGESTION_AVOIDANCE;
                break;
            case CONGESTION_AVOIDANCE:
                if (acknum - last_acknum >= sendq_get_cwnd(sendq) * MAX_PAYLOAD_SIZE) {
                    sendq_inc_cwnd(sendq);
                    last_acknum = acknum;
                }
                break;
            case FAST_RECOVERY:
                sendq_set_cwnd(sendq, sendq_get_ssthresh(sendq));
                state = SLOW_START;
                break;
            }
        } else {
            dupcount++;
            if (dupcount == 3) {
            #ifdef DEBUG
                printf("3 duplicate acks!\n");
            #endif

                // Push the acknum of the duplicate acks onto retransmission
                // queue. sender_thread will take care of the actual
                // retransmission.
                retransq_push(retransq, packet->seqnum);
                sendq_halve_ssthresh(sendq);
                sendq_set_cwnd(sendq, sendq_get_ssthresh(sendq) + 3);
                state = FAST_RECOVERY;
            } else if (dupcount > 3) {
                sendq_inc_cwnd(sendq);
            }
        }
    }

    return NULL;
}
