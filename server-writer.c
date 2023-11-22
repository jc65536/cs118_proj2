#include <stdio.h>

#include "server.h"
#include "compression.h"

static struct recvbuf *recvbuf;
static FILE *fp;

void write_file_(const char *src, size_t size) {
    fwrite(src, sizeof(char), size, fp);
}

size_t read_decompressed(char *dest, size_t size) {
    return recvbuf_take_begin(recvbuf, dest, size);
}

void *write_file(struct writer_args *args) {
    recvbuf = args->recvbuf;

    // Open the target file for writing (always write to output.txt)
    fp = fopen("output.txt", "wb");

    decompress(read_decompressed, write_file_);

    printf("Wrote last packet\n");
    fclose(fp);
    return NULL;
}
