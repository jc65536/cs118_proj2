#include <stdio.h>

#include "server.h"

void *write_file(struct writer_args *args) {
    struct recvq *recvq = args->recvq;

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    while (true) {
        if (recvq->begin == recvq->ack_next)
            continue;

        size_t payload_size = recvq->buf[recvq->begin].payload_size;
        struct packet *packet = &recvq->buf[recvq->begin].packet;

        size_t bytes_written = fwrite(packet->payload, sizeof(char), payload_size, fp);

        if (bytes_written != payload_size)
            perror("Error writing output");

        recvq->begin = (recvq->begin + 1) % RECVQ_CAPACITY;
        recvq->rwnd++;

        if (is_final(packet))
            break;
    }

    fclose(fp);
    return NULL;
}
