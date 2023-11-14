#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

void queue_ack(struct ackq *q, size_t acknum, struct recvq *recvq) {
    if (q->num_queued == ACKQ_CAPACITY) {
        printf("ACK queue full\n");
        return;
    }

    q->buf[q->end].packet_size = HEADER_SIZE;

    struct packet *packet = &q->buf[q->end].packet;

    packet->seqnum = acknum;
    packet->rwnd = recvq->rwnd;

    q->end = (q->end + 1) % ACKQ_CAPACITY;
    q->num_queued++;

    debug_ackq("ACK", packet, q);
}

void queue_nack(struct ackq *q, size_t acknum, struct recvq *recvq) {
    if (q->num_queued == ACKQ_CAPACITY) {
        printf("ACK queue full\n");
        return;
    }

    q->buf[q->end].packet_size = 0;

    struct packet *packet = &q->buf[q->end].packet;

    packet->seqnum = acknum;
    packet->rwnd = recvq->rwnd;

    uint32_t *write_dest = (uint32_t *) packet->payload;
    size_t segnum = acknum;
    for (size_t i = recvq->ack_next; i != recvq->end; i = (i + 1) % RECVQ_CAPACITY) {
        if (recvq->buf[i].filled) {
            segnum += recvq->buf[i].payload_size;
        } else {
            *write_dest = segnum;
            segnum += MAX_PAYLOAD_SIZE;
            write_dest++;
            q->buf[q->end].packet_size += 4;
        }
    }

    q->end = (q->end + 1) % ACKQ_CAPACITY;
    q->num_queued++;

    debug_ackq("NACK", packet, q);
}

void *receive_packets(struct receiver_args *args) {
    struct recvq *recvq = args->recvq;
    struct ackq *ackq = args->ackq;

    // Create a UDP socket for listening
    int listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        exit(1);
    }

    // Configure the server address structure
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        exit(1);
    }

    printf("Connected to proxy (recv)\n");

    struct packet *packet = malloc(sizeof(struct packet));
    size_t acknum = 0;
    size_t endnum = 0;

    while (true) {
        ssize_t bytes_recvd = recv(listen_sockfd, packet, sizeof(struct packet), 0);

        if (bytes_recvd == -1)
            perror("Error receiving message");

        if (packet->seqnum == acknum) {
            memcpy(&recvq->buf[recvq->ack_next].packet, packet, sizeof(struct packet));
            size_t payload_size = bytes_recvd - HEADER_SIZE;
            recvq->buf[recvq->ack_next].payload_size = payload_size;
            recvq->buf[recvq->ack_next].filled = true;

            if (recvq->end == recvq->ack_next) {
                acknum += payload_size;
                endnum += payload_size;
                recvq->ack_next++;
                recvq->end++;
                recvq->rwnd--;
            } else {
                size_t i = recvq->ack_next;
                while (recvq->buf[i].filled) {
                    acknum += recvq->buf[i].payload_size;
                    i = (i + 1) % RECVQ_CAPACITY;
                }
                recvq->ack_next = i;
            }

            debug_recvq("Seq", packet, recvq);

            queue_ack(ackq, acknum, recvq);
        } else if (acknum < packet->seqnum && packet->seqnum < endnum) {
            size_t packet_index = (recvq->ack_next + (packet->seqnum - acknum) / MAX_PAYLOAD_SIZE) % RECVQ_CAPACITY;
            memcpy(&recvq->buf[packet_index].packet, packet, sizeof(struct packet));
            recvq->buf[packet_index].payload_size = bytes_recvd - HEADER_SIZE;
            recvq->buf[packet_index].filled = true;

            debug_recvq("Ret", packet, recvq);
        } else if (endnum <= packet->seqnum && packet->seqnum < endnum + recvq->rwnd * MAX_PAYLOAD_SIZE) {
            size_t rwnd_shrink_count = (packet->seqnum - endnum) / MAX_PAYLOAD_SIZE;
            size_t packet_index = (recvq->end + rwnd_shrink_count) % RECVQ_CAPACITY;
            memcpy(&recvq->buf[packet_index].packet, packet, sizeof(struct packet));
            size_t payload_size = bytes_recvd - HEADER_SIZE;
            recvq->buf[packet_index].payload_size = payload_size;
            recvq->buf[packet_index].filled = true;

            endnum = packet->seqnum + payload_size;
            recvq->rwnd -= rwnd_shrink_count;
            recvq->end = packet_index + 1;

            debug_recvq("Ooo", packet, recvq);

            queue_nack(ackq, acknum, recvq);
        } else {
            // Theoretically shouldn't happen if client respects rwnd
        }
    }

    return NULL;
}
