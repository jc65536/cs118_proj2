#include <stdio.h>
#include <string.h>

#include "client.h"
#include "compression.h"

static FILE *fp;
static struct sendq *sendq;
static bool read_done = false;
uint32_t seqnum = 0;

bool read_packet(struct packet *p, size_t *packet_size) {
    size_t bytes_read = fread(p->payload, sizeof(char), MAX_PAYLOAD_SIZE, fp);

    if (bytes_read != MAX_PAYLOAD_SIZE) {
        if (feof(fp)) {
            p->flags = FLAG_FINAL;
            read_done = true;
        } else {
            perror("Error reading file");
            return false;
        }
    } else {
        p->flags = 0;
    }

    p->seqnum = seqnum;
    *packet_size = HEADER_SIZE + bytes_read;
    seqnum += bytes_read;
    return true;
}

void *read_and_compress(struct reader_args *args) {
    sendq = args->sendq;
    const char *filename = args->filename;

    // Open file for reading
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    printf("Opened file %s\n", filename);

    while (!read_done)
        sendq_write(sendq, read_packet);

    printf("Finished reading file\n");
    return NULL;
}
