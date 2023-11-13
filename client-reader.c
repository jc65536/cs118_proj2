#include <stdio.h>

#include "client.h"

void *read_file(struct reader_args *args) {
    struct sendq *queue = args->queue;
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
        if (queue->num_queued == SENDQ_CAPACITY)
            continue;

        struct packet *packet = queue->buf + queue->end;
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
        seqnum += bytes_read;

        queue->end = (queue->end + 1) % SENDQ_CAPACITY;
        queue->num_queued++;
    }

    printf("Finished reading file\n");
}
