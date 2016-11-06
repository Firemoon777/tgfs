CC=gcc
CFLAGS= -Wall -Werror -pedantic -c `pkg-config fuse --cflags`
SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=%.o)
PROJECT_NAME=tgfs

all: $(OBJ)
	$(CC) -o $(PROJECT_NAME) $(OBJ) `pkg-config fuse --libs`

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf *.o
