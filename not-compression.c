#include <stdlib.h>

#include "not-compression.h"

#define BUF_SIZE 8192

void copy(size_t (*read)(char *, size_t), void (*write)(const char *, size_t)) {
    char buf[BUF_SIZE];
    size_t buf_size;

    while ((buf_size = read(buf, sizeof(buf))))
        write(buf, buf_size);
}
