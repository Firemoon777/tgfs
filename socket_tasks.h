#ifndef _SOCKET_TASKS_H_ 

#define _SOCKET_TASKS_H_ 

void socket_init();
void socket_close();

size_t socket_read_only_size();

int socket_read_data(char** data, size_t* len);
ssize_t socket_send_string(char* string, size_t size);

#endif

