#include <stdio.h>

#include "client.h"

void *read_file(struct reader_args *args) {
    struct sendq *sendq = args->sendq;
    char *filename = args->filename;

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    printf("Opened file %s\n", filename);

    bool eof = false;
    bool set_nonempty = false;
    size_t seqnum = 0;

    while (!eof) {
        if (sendq->num_queued == SENDQ_CAPACITY)
            continue;

        struct packet *packet = &sendq->buf[sendq->end];
        size_t bytes_read = fread(packet->payload, sizeof(char), MAX_PAYLOAD_SIZE, fp);

        if (bytes_read != MAX_PAYLOAD_SIZE) {
            if (feof(fp)) {
                packet->flags = FLAG_FINAL;
                eof = true;
            } else {
                perror("Error reading file");
            }
        } else {
            packet->flags = 0;
        }

        printf("Seqnum: %d\n", seqnum);

        packet->seqnum = seqnum;
        packet->payload_size = bytes_read;
        seqnum += bytes_read;

        sendq->end = (sendq->end + 1) % SENDQ_CAPACITY;
        sendq->num_queued++;
    }

    printf("Finished reading file\n");
    return NULL;
}
