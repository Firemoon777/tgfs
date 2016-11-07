#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include "socket_tasks.h"
#include "string_list.h"
#include "jsmn/jsmn.h"

int socket_init(const char* name) {
	int fd;
	if ( (fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, name);
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("connect error");
		exit(-1);
	}
	return fd;
}

void socket_send_string(int fd, char* string, size_t size) {
	printf("Write status: %li\n", send(fd, string, size, 0));
}

size_t socket_read_answer_size(int fd) {
	size_t result = 0;
	size_t len = 7;
	char* t = (char*)malloc(len*sizeof(char));
	char digit = 0;
	printf("Read status: %li\n", recv(fd, t, len, 0));
	printf("Buffer: %s\n", t);
	while(1) {
		read(fd, &digit, 1);
		if(digit == '\n')
			break;
		result = result * 10 + (digit - '0');
	} 
	free(t);
	return result;
}

void socket_read_answer(int fd, char* data, size_t size) {
	read(fd, data, size);
}
