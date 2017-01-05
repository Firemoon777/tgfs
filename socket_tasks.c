#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include "socket_tasks.h"
#include "string_list.h"

#define SOCKET_NAME "tg_socket"

int socket_fd = -1;

static int socket_open(const char* name) {
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

void socket_init() {
	socket_fd = socket_open(SOCKET_NAME);
}

ssize_t socket_send_string(char* string, size_t size) {
	return send(socket_fd, string, size, 0);
}

static size_t socket_read_answer_size(int fd) {
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

int socket_read_data(char** data, size_t* len) {
	*len = socket_read_answer_size(socket_fd);
	*data = (char*)malloc(*len*sizeof(char));
	recv(socket_fd, *data, *len, 0);
	return 0;
}

