#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"
#include "rto.h"

volatile seqnum_t *holes;
volatile size_t holes_len;

volatile enum trans_state state = SLOW_START;

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

    struct packet *packet = malloc(sizeof(struct packet));
    holes = malloc(MAX_PAYLOAD_SIZE);
    seqnum_t *holes_ = malloc(MAX_PAYLOAD_SIZE);

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

        memcpy(holes_, packet->payload, bytes_recvd - HEADER_SIZE);
        holes_len = (bytes_recvd - HEADER_SIZE) / sizeof(seqnum_t);

        seqnum_t *tmp = (seqnum_t *) holes;
        holes = holes_;
        holes_ = tmp;

        if (holes_len >= 3)
            sendq_sack(sendq, holes + 1, holes_len - 1);

        log_ack(packet->seqnum);

        sendq_pop(sendq, packet->seqnum);

        if (packet->seqnum > acknum) {
            // We received an ack for new data, so we can pop the packets in our
            // buffer until the latest acknum

            dupcount = 0;
            acknum = packet->seqnum;

            if (sendq_get_in_flight(sendq))
                set_timer(timer);
            else
                unset_timer(timer);

            switch (state) {
            case SLOW_START:
                if (sendq_inc_cwnd(sendq) > sendq_get_ssthresh(sendq))
                    state = CONGESTION_AVOIDANCE;
                break;
            case CONGESTION_AVOIDANCE:
                if ((unsigned) (acknum - last_acknum) >= sendq_get_cwnd(sendq)) {
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
            // printf("Received ack %d\n", packet->seqnum);
#endif
            dupcount++;
            if (dupcount == 3) {
#ifdef DEBUG
                printf("3 duplicate acks! (%d)\n", packet->seqnum);
#endif

                // Push the acknum of the duplicate acks onto retransmission
                // queue. sender_thread will take care of the actual
                // retransmission.

                sendq_retrans_holes(sendq, retransq);
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
}
