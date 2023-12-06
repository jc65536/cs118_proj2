#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "compression.h"

#define BUF_SIZE 128

static int fd_out;
static int fd_in;

size_t read_in(char *c, size_t s) {
    return read(fd_in, c, s);
}

void write_out(const char *c, size_t s) {
    write(fd_out, c, s);
}

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;

    fd_in = open(argv[1], O_RDONLY);
    fd_out = creat("compressed.lzw", S_IRUSR | S_IWUSR);

    compress(read_in, write_out);

    printf("====\n");

    close(fd_in);
    close(fd_out);

    fd_in = open("compressed.lzw", O_RDONLY);
    fd_out = creat("decoded.txt", S_IRUSR | S_IWUSR);

    decompress(read_in, write_out);
}
