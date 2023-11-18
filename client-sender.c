#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include "client.h"

static int send_sockfd;

bool send_one(const struct packet *p, size_t packet_size) {
    ssize_t bytes_sent = send(send_sockfd, p, packet_size, 0);

    if (bytes_sent == -1) {
        perror("Error sending message");
        return false;
    }

    return true;
}

void *send_packets(struct sender_args *args) {
    struct sendq *sendq = args->sendq;
    struct retransq *retransq = args->retransq;
    timer_t timer = args->timer;

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

    while (true) {
        if (retransq_pop(retransq, sendq, send_one) ||
            sendq_send_next(sendq, send_one)) {
            if (!is_timer_set(timer))
                set_timer(timer);
        }
    }

    printf("Sent last packet\n");

    return NULL;
}
