CC = gcc

LDFLAGS = -lrt
CFLAGS = -Wall -Wextra

ifneq ($(wildcard .env),)
include .env
endif

# If profiling, add -pg
ifdef PROF
CFLAGS += -pg
LDFLAGS += -pg
endif

ifdef DEBUG
CFLAGS += -g -D DEBUG
else
CFLAGS += -O3
endif

sources = $(wildcard *.c)
objects = $(patsubst %.c,%.o,$(sources))
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

zip: $(sources) $(headers) Makefile README
	zip project2.zip $^
