#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void compress(size_t (*read)(char *, size_t), void (*write)(const char *, size_t));

void decompress(size_t (*read)(char *, size_t), void (*write)(const char *, size_t));

void dummy(size_t (*read)(char *, size_t), void (*write)(const char *, size_t));

#endif
