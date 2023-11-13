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
        if (queue->state == FULL)
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

        mtx_lock(&queue->mutex);
        
        queue->end = (queue->end + 1) % SENDQ_SIZE;

        switch (queue->state) {
        case EMPTY:
            queue->state = NONEMPTY;
            break;
        case NONEMPTY:
            if (queue->end == queue->begin)
                queue->state = FULL;
            break;
        case FULL:  // Would not have gotten here
            break;
        }

        mtx_unlock(&queue->mutex);
    }

    printf("Finished reading file\n");
}
