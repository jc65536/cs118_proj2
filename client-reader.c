#include <stdio.h>

#include "client.h"

void *read_file(struct reader_args *args) {
    struct sendq *sendq = args->sendq;
    const char *filename = args->filename;

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    printf("Opened file %s\n", filename);

    bool eof = false;
    uint32_t seqnum = 0;

    bool _f = true;

    while (!eof) {
        if (sendq->num_queued == SENDQ_CAPACITY) {
            if (_f)
                printf("Send queue full\n");
            _f = false;
            continue;
        }
        _f = true;

        size_t *packet_size = &sendq->buf[sendq->end].packet_size;
        struct packet *packet = &sendq->buf[sendq->end].packet;
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

        packet->seqnum = seqnum;
        *packet_size = HEADER_SIZE + bytes_read;
        seqnum += bytes_read;

        sendq->end = (sendq->end + 1) % SENDQ_CAPACITY;
        sendq->num_queued++;

        printf("Read\tseq %7d\tqueued %3ld\n", packet->seqnum, sendq->num_queued);
    }

    printf("Finished reading file\n");
    return NULL;
}
