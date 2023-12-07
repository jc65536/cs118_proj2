#include <stdio.h>
#include <string.h>

#include "client.h"
#include "not-compression.h"

static FILE *fp;
static struct sendq *sendq;

size_t read_file(char *dest, size_t req_size) {
    return fread(dest, sizeof(char), req_size, fp);
}

void write_compressed(const char *src, size_t size) {
    sendq_fill_end(sendq, src, size);
}

void *read_and_compress(struct reader_args *args) {
    sendq = args->sendq;
    const char *filename = args->filename;

    // Open file for reading
    fp = fopen(filename, "rb");

    copy(read_file, write_compressed);
    sendq_flush_end(sendq, true);
    return NULL;
}
