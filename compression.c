#include "compression.h"

void compress(const char *in, size_t in_size, void (*write)(const char *, size_t)) {
    write(in, in_size);
}

void decompress(const char *in, size_t in_size, void (*write)(const char *, size_t)) {
    write(in, in_size);
}
