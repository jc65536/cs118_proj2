#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"
#include "rto.h"

seqnum_t holes[MAX_PAYLOAD_SIZE / sizeof(seqnum_t)];
size_t holes_len;

enum trans_state state = SLOW_START;

void retrans_holes(struct retransq *q, seqnum_t *holes, size_t holes_len) {
    if (holes_len < 2)
        return;
    
    printf("Queuing hole %d..%d\n", holes[0], holes[1]);

    for (seqnum_t i = holes[0]; i < holes[1]; i++)
        retransq_push(q, i);
    
    retrans_holes(q, holes + 2, holes_len - 2);
}

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
    seqnum_t last_acknum = 0;
    seqnum_t acknum = 0;

    int dupcount = 0;

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1) {
            perror("Error receiving message");
            continue;
        }

        memcpy(holes, packet->payload, bytes_recvd - HEADER_SIZE);
        holes_len = (bytes_recvd - HEADER_SIZE) / sizeof(seqnum_t);

        if (holes_len >= 3)
            sendq_sack(sendq, holes + 1, holes_len - 1);

        log_ack(packet->seqnum);

        if (packet->seqnum > acknum) {
            // We received an ack for new data, so we can pop the packets in our
            // buffer until the latest acknum

            dupcount = 0;

            // sendq_pop returns the number of in-flight packets (packets sent
            // but not yet acked). If this number is 0, we can disarm the timer.
            if (sendq_pop(sendq, packet->seqnum) == 0)
                unset_timer(timer);
            else
                set_timer(timer);

            // Update acknum to the latest acknum
            acknum = packet->seqnum;

            switch (state) {
            case SLOW_START:
                if (sendq_inc_cwnd(sendq) > sendq_get_ssthresh(sendq))
                    state = CONGESTION_AVOIDANCE;
                break;
            case CONGESTION_AVOIDANCE:
                if (acknum - last_acknum >= sendq_get_cwnd(sendq)) {
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
#ifdef DEBUG
            printf("Received ack %d\n", packet->seqnum);
#endif

            dupcount++;
            if (dupcount == 3) {
#ifdef DEBUG
                printf("3 duplicate acks!\n");
#endif

                // Push the acknum of the duplicate acks onto retransmission
                // queue. sender_thread will take care of the actual
                // retransmission.

                retrans_holes(retransq, holes, holes_len);
                holes_len = 0;

                sendq_set_cwnd(sendq, sendq_halve_ssthresh(sendq) + 3);
                state = FAST_RECOVERY;
            } else if (dupcount > 3) {
                sendq_inc_cwnd(sendq);
            } else {
                // Violating RFC 5681 Section 3.2
                // But that assumes a malicious receiver
                sendq_inc_cwnd(sendq);
            }
        }
    }

    return NULL;
}
