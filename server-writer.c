#include <stdio.h>

#include "server.h"

static bool wrote_final;
static FILE *fp;

void write_one(const struct packet *p, size_t payload_size) {
    if (is_final(p))
        wrote_final = true;

    size_t bytes_written = fwrite(p->payload, sizeof(char), payload_size, fp);
    if (bytes_written != payload_size)
        perror("Error writing output");
}

void *write_file(struct writer_args *args) {
    struct recvq *recvq = args->recvq;

    // Open the target file for writing (always write to output.txt)
    fp = fopen("output.txt", "wb");

    wrote_final = false;
    while (!wrote_final)
        recvq_pop(recvq, write_one);

    printf("Wrote last packet\n");
    fclose(fp);
    return NULL;
}
