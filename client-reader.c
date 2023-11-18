#include <stdio.h>

#include "client.h"

static bool read_final;
static FILE *fp;
uint32_t seqnum;

bool read_one(struct packet *p, size_t *packet_size) {
    size_t bytes_read = fread(p->payload, sizeof(char), MAX_PAYLOAD_SIZE, fp);

    if (bytes_read != MAX_PAYLOAD_SIZE) {
        if (feof(fp)) {
            p->flags = FLAG_FINAL;
            read_final = true;
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

void *read_file(struct reader_args *args) {
    struct sendq *sendq = args->sendq;
    const char *filename = args->filename;

    // Open file for reading
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    printf("Opened file %s\n", filename);

    read_final = false;
    seqnum = 0;
    while (!read_final)
        sendq_write(sendq, read_one);

    printf("Finished reading file\n");
    return NULL;
}
