#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "compression.h"

#define BUF_SIZE 128

static int fd_out;
static int fd_in;

size_t read_in(char *c, size_t s) {
    return read(fd_in, c, s);
}

void write_out_print(const char *c, size_t s) {
    printf("Encode: ");
    for (int i = 0; i < s; i++)
        printf("%02x ", c[i] & 0xff);
    printf("\n");
    write(fd_out, c, s);
}

void write_out(const char *c, size_t s) {
    write(fd_out, c, s);
}

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;

    fd_in = open(argv[1], O_RDONLY);
    fd_out = creat("compressed.lzw", S_IRUSR | S_IWUSR);

    compress(read_in, write_out_print);
    
    close(fd_in);
    close(fd_out);

    fd_in = open("compressed.lzw", O_RDONLY);
    fd_out = creat("decoded.txt", S_IRUSR | S_IWUSR);

    decompress(read_in, write_out);
}
