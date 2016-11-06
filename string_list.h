#ifndef _STRING_LIST_H_ 

#define _STRING_LIST_H_ 

typedef struct string_list {
	char* name;
	struct string_list* next;
} string_list;

string_list* string_list_init(char* name);

void string_list_add_front(string_list** l, char* name);

#endif
