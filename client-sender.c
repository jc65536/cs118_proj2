#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include "client.h"

static int send_sockfd;
static struct sendq *sendq;

bool send_one(const struct packet *p, size_t packet_size) {
    send(send_sockfd, p, packet_size, 0);
    return true;
}

bool send_seqnum(uint32_t n) {
    sendq_lookup_seqnum(sendq, n, send_one);
    return true;
}

void *send_packets(struct sender_args *args) {
    sendq = args->sendq;
    struct retransq *retransq = args->retransq;
    timer_t timer = args->timer;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Configure the server address structure to which we will send data
    struct sockaddr_in server_addr_to = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT_TO),
        .sin_addr.s_addr = inet_addr(SERVER_IP)};

    connect(send_sockfd, (struct sockaddr *) &server_addr_to, sizeof(server_addr_to));

    while (true) {
        if (retransq_pop(retransq, send_seqnum) ||
            sendq_send_next(sendq, send_one)) {
            if (!is_timer_set(timer))
                set_timer(timer);
        }
    }
}
