#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stddef.h>
#include <stdbool.h>

void compress(const char *in, size_t in_size,
              void (*write)(const char *, size_t));

void decompress(const char *in, size_t in_size,
                void (*write)(const char *, size_t));

#endif
