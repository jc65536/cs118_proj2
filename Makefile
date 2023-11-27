CC = gcc

LDFLAGS = -lrt -g
CFLAGS = -Wall -Wextra -g

# If profiling, add -pg
ifdef PROF
CFLAGS += -p
LDFLAGS += -p
endif

ifdef OPT
CFLAGS += -O3
endif

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
		server-sender.o \
		compression.o

client: client.o common.o \
		client-timer.o \
		client-queues.o \
		client-reader.o \
		client-sender.o \
		client-receiver.o \
		compression.o

comp-test: comp-test.o compression.o

clean:
	rm -f server client comp-test output.txt project2.zip *.o

zip: 
	zip project2.zip server.c client.c utils.h Makefile README
