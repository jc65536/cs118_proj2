#include <stdio.h>

#include "compression.h"
#include "server.h"

static struct recvbuf *recvbuf;
static FILE *fp;

void write_file(const char *src, size_t size) {
    fwrite(src, sizeof(char), size, fp);
}

size_t read_compressed(char *dest, size_t size) {
    return recvbuf_take_begin(recvbuf, dest, size);
}

void *decompress_and_write(struct writer_args *args) {
    recvbuf = args->recvbuf;

    // Open the target file for writing (always write to output.txt)
    fp = fopen("output.txt", "wb");

    decompress(read_compressed, write_file);

    printf("Wrote last packet\n");
    fclose(fp);
    exit(0);
    return NULL;
}
