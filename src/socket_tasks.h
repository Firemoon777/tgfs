#ifndef _SOCKET_TASKS_H_ 

#define _SOCKET_TASKS_H_ 

#define SOCKET_NAME "tg_socket"
#define BUFFER_SIZE 1000
#define MAX_FILEPATH_SIZE 4096


void socket_init();
void socket_close();

int socket_read_data(char** data, size_t* len);
ssize_t socket_send_string(char* string, size_t size);

void get_home_dir(char* home);
void get_downloads_dir(char* dir);

#endif

