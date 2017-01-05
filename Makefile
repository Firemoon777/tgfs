CC=gcc
CFLAGS= -Wall -Werror -pedantic -c -std=c99 `pkg-config fuse --cflags` -O3 -D DEBUG
SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=%.o)
PROJECT_NAME=tgfs

all: $(OBJ) jsmn.o
	$(CC) -o $(PROJECT_NAME) $(OBJ) jsmn.o `pkg-config fuse --libs`

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

jsmn.o: jsmn/jsmn.c
	$(CC) $(CFLAGS) -o $@ $<
	
clean:
	rm -rf *.o

mount: all
	./$(PROJECT_NAME) -f -s -o direct_io test

daemon-start:
	telegram-cli -d -vvvv -S tg_socket --json -L /var/log/telegram-daemon/telegram-cli.log &
