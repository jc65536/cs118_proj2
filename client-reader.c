#include <stdio.h>
#include <string.h>

#include "client.h"

static FILE *fp;
static struct sendq *sendq;
static bool read_done = false;
seqnum_t seqnum = 0;

bool read_packet(struct packet *p, size_t *packet_size) {
    size_t bytes_read = fread(p->payload, sizeof(char), MAX_PAYLOAD_SIZE, fp);

    if (bytes_read != MAX_PAYLOAD_SIZE) {
        if (feof(fp)) {
            p->flags = FLAG_FINAL;
            read_done = true;
        }
    } else {
        p->flags = 0;
    }

    p->seqnum = seqnum;
    *packet_size = HEADER_SIZE + bytes_read;
    seqnum++;
    return true;
}

void *read_file(struct reader_args *args) {
    sendq = args->sendq;
    const char *filename = args->filename;

    // Open file for reading
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    while (!read_done)
        sendq_write(sendq, read_packet);

    fclose(fp);
    return NULL;
}
