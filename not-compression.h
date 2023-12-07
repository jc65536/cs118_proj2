#ifndef NOT_COMPRESSION_H
#define NOT_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

void copy(size_t (*read)(char *, size_t), void (*write)(const char *, size_t));

#endif
