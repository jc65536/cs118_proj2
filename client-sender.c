#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "rto.h"

static int send_sockfd;
static struct sendq *sendq;
static timer_t timer;

bool send_one(const struct packet *p, size_t packet_size) {
    ssize_t bytes_sent = send(send_sockfd, p, packet_size, 0);

    if (bytes_sent == -1) {
        perror("Error sending message");
        return false;
    }

    log_send(p->seqnum);

    if (!is_timer_set(timer))
        set_timer(timer);
    return true;
}

bool send_seqnum(seqnum_t n) {
    sendq_lookup_seqnum(sendq, n, send_one);
    return true;
}

void *send_packets(struct sender_args *args) {
    sendq = args->sendq;
    struct retransq *retransq = args->retransq;
    timer = args->timer;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    while (true)
        (void) (retransq_pop(retransq, send_seqnum) || sendq_send_next(sendq, send_one));

    printf("Sent last packet\n");

    return NULL;
}
