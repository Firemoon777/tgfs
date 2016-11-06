#include "string_list.h"
#include <malloc.h>
#include <string.h>

string_list* string_list_init(char* name) {
	string_list* result = (string_list*)malloc(sizeof(string_list));
	result->name = (char*)malloc((1+strlen(name))*sizeof(char));
	strcpy(result->name, name);
	result->next = NULL;
	return result;
}

void string_list_add_front(string_list** l, char* name) {
	if(name == NULL) {
		return;
	}
	string_list* result = string_list_init(name);
	result->next = *l;
	*l = result;
}
