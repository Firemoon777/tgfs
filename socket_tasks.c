#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <sys/types.h>
#include <pwd.h>

#include "socket_tasks.h"

#define SOCKET_NAME "tg_socket"
#define BUFFER_SIZE 1000

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

void get_home_dir(char* home) {
	struct passwd *pw = getpwuid(getuid());
	strcpy(home, pw->pw_dir);
	strcat(home, "/.tgfs/");
}

void get_downloads_dir(char* dir) {
	get_home_dir(dir);
	strcat(dir, "donwloads/");
}

void socket_init() {
	system("telegram-tgfs -d -vvvv -S ~/.tgfs/"SOCKET_NAME" --json --permanent-msg-ids --permanent-peer-ids -L /var/log/telegram-daemon/telegram-cli.log &");
	sleep(1);
	
	char socket_name[255];
	get_home_dir(socket_name);
	strcat(socket_name, SOCKET_NAME);
	socket_fd = socket_open(socket_name);
}

void socket_close() {
	shutdown(socket_fd, 2);
	char home[255];
	get_home_dir(home);
	strcat(home, SOCKET_NAME);
	system("pkill -9 telegram-tgfs");
	remove(home);
	system("rm -r ~/.tgfs/downloads/");
}

ssize_t socket_send_string(char* string, size_t size) {
	return send(socket_fd, string, size, 0);
}

static size_t socket_read_answer_size(int fd) {
	size_t result = 0;
	size_t len = 7;
	char* t = (char*)malloc(len*sizeof(char));
	char digit = 0;
	if(recv(fd, t, len, 0) < 0) 
		return -1;
	while(1) {
		read(fd, &digit, 1);
		if(digit == '\n')
			break;
		result = result * 10 + (digit - '0');
	} 
	free(t);
	return result;
}

size_t socket_read_only_size() {
	size_t result = socket_read_answer_size(socket_fd);
	recv(socket_fd, NULL, result + 1, 0);
	return result;
}

int socket_read_data(char** data, size_t* len) {
	*len = socket_read_answer_size(socket_fd);
	size_t size = (*len) + 1;
	*data = (char*)malloc(size*sizeof(char));
	assert(*data);
	size_t result;
	for(size_t i = 0; i < size; i += result) {
		result = recv(socket_fd, *data + i, BUFFER_SIZE, 0);
	}
	(*data)[*len] = 0;
	return 0;
}

