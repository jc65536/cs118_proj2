#include <stdio.h>

#include "server.h"

static struct recvbuf *recvbuf;
static FILE *fp;
static bool write_done;

bool write_one(const struct packet *p, size_t payload_size) {
    write_done = is_final(p);

    size_t bytes_written = fwrite(p->payload, sizeof(char), payload_size, fp);

    if (bytes_written != payload_size) {
        perror("Error writing output");
        return false;
    }

    return true;
}

void *write_file(struct writer_args *args) {
    recvbuf = args->recvbuf;

    // Open the target file for writing (always write to output.txt)
    fp = fopen("output.txt", "wb");

    while (!write_done)
        recvbuf_pop(recvbuf, write_one);

    fclose(fp);
    return NULL;
}
