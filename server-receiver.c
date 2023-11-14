#include <arpa/inet.h>
#include <unistd.h>

#include "server.h"

void queue_ack(struct ackq *q, size_t acknum, uint16_t rwnd) {
    if (q->num_queued == ACKQ_CAPACITY) {
        printf("ACK queue full\n");
        return;
    }

    q->buf[q->end].packet_size = HEADER_SIZE;
    q->buf[q->end].packet.seqnum = acknum;
    q->buf[q->end].packet.recv_window = rwnd;

    q->end = (q->end + 1) % ACKQ_CAPACITY;
    q->num_queued++;
}

void queue_nack(struct ackq *q, size_t acknum, uint16_t rwnd,
                struct recvq_slot *ack_next, struct recvq_slot *end) {
    if (q->num_queued == ACKQ_CAPACITY) {
        printf("ACK queue full\n");
        return;
    }

    q->buf[q->end].packet_size = 0;
    q->buf[q->end].packet.seqnum = acknum;
    q->buf[q->end].packet.recv_window = rwnd;

    uint32_t *write_dest = (uint32_t *) q->buf[q->end].packet.payload;
    size_t segnum = acknum;
    for (struct recvq_slot *i = ack_next; i != end; i++) {
        if (i->filled) {
            segnum += i->payload_size;
        } else {
            *write_dest = segnum;
            segnum += MAX_PAYLOAD_SIZE;
            write_dest++;
            q->buf[q->end].packet_size += 4;
        }
    }

    q->end = (q->end + 1) % ACKQ_CAPACITY;
    q->num_queued++;
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
        else
            print_recv(packet);

        if (packet->seqnum == acknum) {
            printf("Sequential receive\n");
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

            queue_ack(ackq, acknum, recvq->rwnd);

        } else if (acknum < packet->seqnum && packet->seqnum < endnum) {
            printf("Retransmit\n");
            size_t packet_index = (recvq->ack_next + (packet->seqnum - acknum) / MAX_PAYLOAD_SIZE) % RECVQ_CAPACITY;
            memcpy(&recvq->buf[packet_index].packet, packet, sizeof(struct packet));
            recvq->buf[packet_index].payload_size = bytes_recvd - HEADER_SIZE;
            recvq->buf[packet_index].filled = true;
        } else if (endnum <= packet->seqnum && packet->seqnum < endnum + recvq->rwnd * MAX_PAYLOAD_SIZE) {
            printf("Out of order receive\n");
            size_t packet_index = (recvq->end + (packet->seqnum - endnum) / MAX_PAYLOAD_SIZE) % RECVQ_CAPACITY;
            memcpy(&recvq->buf[packet_index].packet, packet, sizeof(struct packet));
            size_t payload_size = bytes_recvd - HEADER_SIZE;
            recvq->buf[packet_index].payload_size = payload_size;
            recvq->buf[packet_index].filled = true;

            endnum = packet->seqnum + payload_size;
            recvq->rwnd -= packet_index + 1 - recvq->end;
            recvq->end = packet_index + 1;

            queue_nack(ackq, acknum, recvq->rwnd, &recvq->buf[recvq->ack_next], &recvq->buf[recvq->end]);
        } else {
            // Theoretically shouldn't happen if client respects rwnd
        }
    }

    return NULL;
}
