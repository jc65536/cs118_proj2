CC = gcc
LDFLAGS = -lrt
CFLAGS = -Wall -Wextra -g

objects = $(patsubst %.c,%.o,$(wildcard *.c))
headers = $(wildcard *.h)

default: build

build: server client

$(objects): $(headers)

server: server.o common.o \
		server-queues.o \
		server-receiver.o \
		server-copier.o \
		server-writer.o \
		server-sender.o

client: client.o common.o \
		client-timer.o \
		client-queues.o \
		client-reader.o \
		client-sender.o \
		client-receiver.o

clean:
	rm -f server client output.txt project2.zip *.o

zip: 
	zip project2.zip server.c client.c utils.h Makefile README
