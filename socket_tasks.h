#ifndef _SOCKET_TASKS_H_ 

#define _SOCKET_TASKS_H_ 

int socket_init(const char* name);

size_t socket_read_answer_size(int fd);

void socket_send_string(int fd, char* string, size_t size);

void socket_read_answer(int fd, char* data, size_t size);

#endif

